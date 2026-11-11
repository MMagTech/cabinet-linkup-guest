// The window's comings and goings on a Mac (tray.h), as Messages and Slack have them: the red button
// closes the window and the app stays in the Dock, whose icon brings it back. SDL's own menu already
// quits on Command-Q, and with SDL told not to quit when its last window closes, closing is hiding.
//
// The Dock icon's click is the reopen Apple event, taken here from AppKit, which would otherwise only
// bring back a minimized window. A second copy started with the same folder finds it held (a lock on
// a file in it) and asks the first, by a distributed notification named for the folder, to show
// itself.
//
// An invite while the window is not in front is a notification from the Notification Centre, which
// macOS gives only to an app with a bundle: until 2P Start is a .app (piece F), the Dock icon bounces
// instead, and opens Join or Decline.

#include "app/tray.h"

#import <AppKit/AppKit.h>
#import <UserNotifications/UserNotifications.h>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <filesystem>
#include <mutex>

#include "app/tray_system.h"

namespace {

NSString* const kOpenName = @"io.github.mmagtech.2pstart.open";

NSString* text(const std::string& s) { return [NSString stringWithUTF8String:s.c_str()] ?: @""; }

// UNUserNotificationCenter throws for a program that is not inside a .app.
bool bundled() {
    NSBundle* b = [NSBundle mainBundle];
    return b.bundleIdentifier != nil && [b.bundleURL.pathExtension isEqualToString:@"app"];
}

}  // namespace

struct tray::Tray::System {
    uint32_t event = 0;
    NSString* key = nil;
    id helper = nil;
    bool notifications = false;
    std::mutex mu;
    std::string answer;     // the friend a clicked notification named
    std::string bounced;    // the friend the Dock icon last bounced for, opened by its click
};

@interface TwoPStartTray : NSObject <UNUserNotificationCenterDelegate>
@property(nonatomic, assign) tray::Tray::System* system;
@end

@implementation TwoPStartTray

// The Dock icon, clicked while the app runs: the window, or Join or Decline after a bounce.
- (void)reopen:(NSAppleEventDescriptor*)event withReplyEvent:(NSAppleEventDescriptor*)reply {
    (void)event;
    (void)reply;
    tray::Tray::System* s = self.system;
    std::lock_guard<std::mutex> lock(s->mu);
    if (!s->bounced.empty()) {
        s->answer = s->bounced;
        s->bounced.clear();
        tray::push(s->event, tray::Ask::Answer);
    } else {
        tray::push(s->event, tray::Ask::Open);
    }
}

// Another copy, started with this folder.
- (void)another:(NSNotification*)note {
    (void)note;
    tray::push(self.system->event, tray::Ask::Open);
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
    didReceiveNotificationResponse:(UNNotificationResponse*)response
             withCompletionHandler:(void (^)(void))done {
    (void)center;
    NSString* who = response.notification.request.content.userInfo[@"who"];
    tray::Tray::System* s = self.system;
    {
        std::lock_guard<std::mutex> lock(s->mu);
        s->answer = who ? std::string(who.UTF8String) : std::string();
    }
    tray::push(s->event, who ? tray::Ask::Answer : tray::Ask::Open);
    done();
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
       willPresentNotification:(UNNotification*)notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions))done {
    (void)center;
    (void)notification;
    done(UNNotificationPresentationOptionBanner | UNNotificationPresentationOptionSound);
}

@end

namespace tray {

bool another(const std::string& folder, bool quietly) {
    std::error_code e;
    std::filesystem::create_directories(folder, e);
    const std::string file = folder + "/running.lock";
    // Held, never closed, until the program ends: the system lets go of it then, however it ends.
    const int fd = open(file.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0) return false;
    if (flock(fd, LOCK_EX | LOCK_NB) == 0) return false;
    close(fd);
    if (!quietly) {
        [[NSDistributedNotificationCenter defaultCenter] postNotificationName:kOpenName
                                                                       object:text(folderKey(folder))
                                                                     userInfo:nil
                                                           deliverImmediately:YES];
    }
    return true;
}

Tray::Tray(SDL_Window* window, const std::string& folder, Asks asks)
    : window_(window), asks_(std::move(asks)), system_(std::make_unique<System>()) {
    System& s = *system_;
    s.event = SDL_RegisterEvents(1);
    s.key = text(folderKey(folder));
    TwoPStartTray* helper = [[TwoPStartTray alloc] init];
    helper.system = &s;
    s.helper = helper;
    // 'aevt' 'rapp': kCoreEventClass, kAEReopenApplication.
    [[NSAppleEventManager sharedAppleEventManager] setEventHandler:helper
                                                       andSelector:@selector(reopen:withReplyEvent:)
                                                     forEventClass:'aevt'
                                                        andEventID:'rapp'];
    [[NSDistributedNotificationCenter defaultCenter] addObserver:helper
                                                        selector:@selector(another:)
                                                            name:kOpenName
                                                          object:s.key];
    s.notifications = bundled();
    if (s.notifications) {
        UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
        center.delegate = helper;
        [center requestAuthorizationWithOptions:UNAuthorizationOptionAlert | UNAuthorizationOptionSound
                              completionHandler:^(BOOL, NSError*){
                              }];
    }
}

Tray::~Tray() {
    System& s = *system_;
    [[NSDistributedNotificationCenter defaultCenter] removeObserver:s.helper];
    [[NSAppleEventManager sharedAppleEventManager] removeEventHandlerForEventClass:'aevt' andEventID:'rapp'];
    if (s.notifications) [UNUserNotificationCenter currentNotificationCenter].delegate = nil;
}

uint32_t eventOf(const Tray::System& s) { return s.event; }

// Out of full screen first, as Messages does, so no empty Space is left behind. The app goes back to
// full screen when the window is shown again.
void hide(Tray::System&, SDL_Window* window, const Asks&) {
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) {
        SDL_SetWindowFullscreen(window, false);
        SDL_SyncWindow(window);
    }
    SDL_HideWindow(window);
}

void notify(Tray::System& s, const std::string& who, const std::string& title, const std::string& game) {
    if (!s.notifications) {
        {
            std::lock_guard<std::mutex> lock(s.mu);
            s.bounced = who;
        }
        [NSApp requestUserAttention:NSInformationalRequest];
        return;
    }
    UNMutableNotificationContent* content = [[UNMutableNotificationContent alloc] init];
    content.title = text(title);
    content.body = text(game);
    content.sound = [UNNotificationSound defaultSound];
    content.userInfo = @{@"who" : text(who)};
    NSString* name = [@"invite-" stringByAppendingString:text(who)];
    UNNotificationRequest* request = [UNNotificationRequest requestWithIdentifier:name content:content trigger:nil];
    [[UNUserNotificationCenter currentNotificationCenter] addNotificationRequest:request withCompletionHandler:nil];
}

std::string takeAnswer(Tray::System& s) {
    std::lock_guard<std::mutex> lock(s.mu);
    std::string who;
    who.swap(s.answer);
    return who;
}

}  // namespace tray
