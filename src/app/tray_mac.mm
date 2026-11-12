// The window's comings and goings on a Mac (tray.h), as a background tool has them: an icon in the
// menu bar, whose click opens the menu, for as long as the app runs; the Dock icon only while the
// window is open. SDL's own menu quits on Command-Q, and with SDL told not to quit when its last
// window closes, closing is hiding.
//
// The menu bar's icon is a template the system colours, as every menu bar icon is: the cabinet in the
// menu bar's own colour, with the status drawn into it in colour (assets/menubar, from icon/make.swift).
// The Dock icon's click is the reopen Apple event, taken here from AppKit, which would otherwise only
// bring back a minimized window. A second copy started with the same folder finds it held (a lock on
// a file in it) and asks the first, by a distributed notification named for the folder, to show
// itself.
//
// An invite while the window is not in front is a notification from the Notification Centre, with
// Join and Decline on it, which macOS gives only to a program inside a .app. Run as a bare program, as
// it is while it is worked on, the Dock icon bounces instead, and its click opens Join or Decline.

#include "app/tray.h"

#import <AppKit/AppKit.h>
#import <UserNotifications/UserNotifications.h>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <cstdio>
#include <filesystem>
#include <mutex>

#include "app/tray_system.h"

namespace {

NSString* const kOpenName = @"io.github.mmagtech.2pstart.open";
NSString* const kInviteKind = @"invite";
NSString* const kJoin = @"join";
NSString* const kDecline = @"decline";
constexpr CGFloat kIconPoints = 18;

NSString* text(const std::string& s) { return [NSString stringWithUTF8String:s.c_str()] ?: @""; }

std::string words(NSString* s) { return s ? std::string(s.UTF8String) : std::string(); }

// UNUserNotificationCenter throws for a program that is not inside a .app.
bool bundled() {
    NSBundle* b = [NSBundle mainBundle];
    return b.bundleIdentifier != nil && [b.bundleURL.pathExtension isEqualToString:@"app"];
}

// Whether macOS shows this app's notifications now: the person can say no when it first asks, or
// turn them off in System Settings at any time. Asked each time; it answers in a moment.
bool allowedNow() {
    __block bool allowed = false;
    dispatch_semaphore_t answered = dispatch_semaphore_create(0);
    [[UNUserNotificationCenter currentNotificationCenter]
        getNotificationSettingsWithCompletionHandler:^(UNNotificationSettings* settings) {
            allowed = settings.authorizationStatus == UNAuthorizationStatusAuthorized ||
                      settings.authorizationStatus == UNAuthorizationStatusProvisional;
            dispatch_semaphore_signal(answered);
        }];
    if (dispatch_semaphore_wait(answered, dispatch_time(DISPATCH_TIME_NOW, 500 * NSEC_PER_MSEC)) != 0) return false;
    return allowed;
}

// One of the menu bar's pictures, both sizes, from beside the fonts.
NSImage* picture(NSString* name) {
    const char* base = SDL_GetBasePath();
    NSString* folder = [text(base ? base : "") stringByAppendingString:@"assets/menubar/"];
    NSImage* image = [[NSImage alloc] initWithSize:NSMakeSize(kIconPoints, kIconPoints)];
    for (NSString* file in @[ [name stringByAppendingString:@".png"], [name stringByAppendingString:@"@2x.png"] ]) {
        NSImageRep* rep = [NSImageRep imageRepWithContentsOfFile:[folder stringByAppendingString:file]];
        if (!rep) continue;
        rep.size = NSMakeSize(kIconPoints, kIconPoints);
        [image addRepresentation:rep];
    }
    return image.representations.count > 0 ? image : nil;
}

NSColor* colourOf(tray::Status status) {
    // As icon/make.swift draws them: green, and the console's amber for an invite.
    if (status == tray::Status::Online) return [NSColor colorWithSRGBRed:0x30 / 255.0 green:0xD1 / 255.0 blue:0x58 / 255.0 alpha:1];
    return [NSColor colorWithSRGBRed:0xFF / 255.0 green:0xC4 / 255.0 blue:0x57 / 255.0 alpha:1];
}

}  // namespace

@class TwoPStartTray;

struct tray::Tray::System {
    uint32_t event = 0;
    SDL_Window* window = nullptr;
    const Asks* asks = nullptr;
    NSString* key = nil;
    TwoPStartTray* helper = nil;
    NSStatusItem* item = nil;
    NSImage* silhouette = nil;   // the template: the cabinet in black
    NSImage* cut = nil;          // what is cut out of it round the status
    NSImage* lit = nil;          // where the status's colour goes
    bool notifications = false;
    std::mutex mu;
    Reply reply;            // what a click asked for
    std::string bounced;    // the friend the Dock icon last bounced for, opened by its click
};

@interface TwoPStartTray : NSObject <UNUserNotificationCenterDelegate, NSMenuDelegate>
@property(nonatomic, assign) tray::Tray::System* system;
@end

@implementation TwoPStartTray

- (void)ask:(tray::Ask)ask {
    tray::push(self.system->event, ask);
}

- (void)answer:(const std::string&)who with:(int)answer {
    tray::Tray::System* s = self.system;
    {
        std::lock_guard<std::mutex> lock(s->mu);
        s->reply = tray::Reply{who, answer};
    }
    tray::push(s->event, tray::Ask::Reply);
}

// The Dock icon, clicked while the app runs: the window, or Join or Decline after a bounce.
- (void)reopen:(NSAppleEventDescriptor*)event withReplyEvent:(NSAppleEventDescriptor*)reply {
    (void)event;
    (void)reply;
    tray::Tray::System* s = self.system;
    std::string who;
    {
        std::lock_guard<std::mutex> lock(s->mu);
        who.swap(s->bounced);
    }
    if (!who.empty()) [self answer:who with:-1];
    else [self ask:tray::Ask::Open];
}

// Another copy, started with this folder.
- (void)another:(NSNotification*)note {
    (void)note;
    [self ask:tray::Ask::Open];
}

// The menu, made afresh each time it opens, from what the app says now.
- (void)menuNeedsUpdate:(NSMenu*)menu {
    [menu removeAllItems];
    const tray::Asks& asks = *self.system->asks;
    const std::vector<tray::Invite> invites = asks.invites ? asks.invites() : std::vector<tray::Invite>{};
    for (const tray::Invite& i : invites) {
        NSMenuItem* m = [menu addItemWithTitle:text(tray::lineOf(i)) action:@selector(invite:) keyEquivalent:@""];
        m.target = self;
        m.representedObject = text(i.who);
    }
    if (!invites.empty()) [menu addItem:[NSMenuItem separatorItem]];

    const bool online = asks.online && asks.online();
    NSMenu* status = [[NSMenu alloc] initWithTitle:@"Status"];
    NSMenuItem* on = [status addItemWithTitle:@"Online" action:@selector(online:) keyEquivalent:@""];
    on.target = self;
    on.state = online ? NSControlStateValueOn : NSControlStateValueOff;
    NSMenuItem* off = [status addItemWithTitle:@"Offline" action:@selector(offline:) keyEquivalent:@""];
    off.target = self;
    off.state = online ? NSControlStateValueOff : NSControlStateValueOn;
    NSMenuItem* statusItem = [menu addItemWithTitle:@"Status" action:nil keyEquivalent:@""];
    statusItem.submenu = status;

    NSMenuItem* open = [menu addItemWithTitle:@"Open 2P Start" action:@selector(openWindow:) keyEquivalent:@""];
    open.target = self;
    NSMenuItem* startup = [menu addItemWithTitle:@"Startup" action:@selector(startup:) keyEquivalent:@""];
    startup.target = self;
    startup.state = asks.startup && asks.startup() ? NSControlStateValueOn : NSControlStateValueOff;
    startup.enabled = !asks.canStartup || asks.canStartup();
    [menu addItem:[NSMenuItem separatorItem]];
    NSMenuItem* quit = [menu addItemWithTitle:@"Quit" action:@selector(quit:) keyEquivalent:@""];
    quit.target = self;
}

- (void)invite:(NSMenuItem*)item {
    [self answer:words(item.representedObject) with:-1];
}

- (void)online:(NSMenuItem*)item {
    (void)item;
    [self ask:tray::Ask::Online];
}

- (void)offline:(NSMenuItem*)item {
    (void)item;
    [self ask:tray::Ask::Offline];
}

- (void)openWindow:(NSMenuItem*)item {
    (void)item;
    [self ask:tray::Ask::Open];
}

- (void)startup:(NSMenuItem*)item {
    (void)item;
    [self ask:tray::Ask::Startup];
}

- (void)quit:(NSMenuItem*)item {
    (void)item;
    SDL_Event e{};
    e.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&e);
}

// A notification's Join or Decline, or a click on the rest of it.
- (void)userNotificationCenter:(UNUserNotificationCenter*)center
    didReceiveNotificationResponse:(UNNotificationResponse*)response
             withCompletionHandler:(void (^)(void))done {
    (void)center;
    NSString* who = response.notification.request.content.userInfo[@"who"];
    if (!who) {
        [self ask:tray::Ask::Open];
    } else if ([response.actionIdentifier isEqualToString:kJoin]) {
        [self answer:words(who) with:0];
    } else if ([response.actionIdentifier isEqualToString:kDecline]) {
        [self answer:words(who) with:1];
    } else if (![response.actionIdentifier isEqualToString:UNNotificationDismissActionIdentifier]) {
        [self answer:words(who) with:-1];
    }
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

namespace {

// The menu bar's picture for `status`: the template alone while Offline, so the system draws it as
// any other; otherwise the cabinet in the menu bar's own colour, cut round the status, and the status
// in its colour.
NSImage* pictureFor(Tray::System& s, Status status) {
    if (!s.silhouette) return nil;
    if (status == Status::Offline || !s.lit) {
        [s.silhouette setTemplate:YES];
        return s.silhouette;
    }
    NSImage* silhouette = s.silhouette;
    NSImage* cut = s.cut;
    NSImage* lit = s.lit;
    NSColor* colour = colourOf(status);
    NSImage* image = [NSImage imageWithSize:NSMakeSize(kIconPoints, kIconPoints)
                                    flipped:NO
                             drawingHandler:^BOOL(NSRect r) {
                                 [silhouette drawInRect:r];
                                 [[NSColor labelColor] set];
                                 NSRectFillUsingOperation(r, NSCompositingOperationSourceIn);
                                 if (cut) {
                                     [cut drawInRect:r
                                            fromRect:NSZeroRect
                                           operation:NSCompositingOperationDestinationOut
                                            fraction:1];
                                 }
                                 NSRect at = r;
                                 CGImageRef mask = [lit CGImageForProposedRect:&at
                                                                        context:[NSGraphicsContext currentContext]
                                                                          hints:nil];
                                 CGContextRef g = [NSGraphicsContext currentContext].CGContext;
                                 CGContextSaveGState(g);
                                 CGContextClipToMask(g, NSRectToCGRect(r), mask);
                                 [colour set];
                                 NSRectFill(r);
                                 CGContextRestoreGState(g);
                                 return YES;
                             }];
    [image setTemplate:NO];
    return image;
}

void dockIcon(bool shown) {
    [NSApp setActivationPolicy:shown ? NSApplicationActivationPolicyRegular : NSApplicationActivationPolicyAccessory];
}

void say(Tray::System& s, NSString* name, NSString* title, NSString* body, NSString* who) {
    UNMutableNotificationContent* content = [[UNMutableNotificationContent alloc] init];
    content.title = title;
    content.body = body;
    content.sound = [UNNotificationSound defaultSound];
    if (who) {
        content.userInfo = @{@"who" : who};
        content.categoryIdentifier = kInviteKind;
    }
    UNNotificationRequest* request = [UNNotificationRequest requestWithIdentifier:name content:content trigger:nil];
    [[UNUserNotificationCenter currentNotificationCenter] addNotificationRequest:request withCompletionHandler:nil];
    (void)s;
}

}  // namespace

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
    s.window = window;
    s.asks = &asks_;
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

    s.silhouette = picture(@"menubar");
    s.cut = picture(@"menubar-cut");
    s.lit = picture(@"menubar-lit");
    s.item = [[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength];
    NSMenu* menu = [[NSMenu alloc] initWithTitle:@"2P Start"];
    menu.autoenablesItems = NO;
    menu.delegate = helper;
    s.item.menu = menu;
    look(s, Status::Offline, tipOf({}, false));
    // Opened at login, there is no window, and so no Dock icon.
    dockIcon(!(SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN));

    s.notifications = bundled();
    if (s.notifications) {
        UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
        center.delegate = helper;
        UNNotificationAction* join = [UNNotificationAction actionWithIdentifier:kJoin
                                                                         title:@"Join"
                                                                       options:UNNotificationActionOptionForeground];
        UNNotificationAction* decline = [UNNotificationAction actionWithIdentifier:kDecline
                                                                            title:@"Decline"
                                                                          options:UNNotificationActionOptionNone];
        UNNotificationCategory* invite =
            [UNNotificationCategory categoryWithIdentifier:kInviteKind
                                                   actions:@[ join, decline ]
                                         intentIdentifiers:@[]
                                                   options:UNNotificationCategoryOptionNone];
        [center setNotificationCategories:[NSSet setWithObject:invite]];
        // What macOS answered goes in the log.
        [center requestAuthorizationWithOptions:UNAuthorizationOptionAlert | UNAuthorizationOptionSound
                              completionHandler:^(BOOL allowed, NSError* error) {
                                  if (error)
                                      std::fprintf(stderr, "[tray] notifications: %s\n",
                                                   error.localizedDescription.UTF8String);
                                  else
                                      std::fprintf(stderr, "[tray] notifications %s\n", allowed ? "allowed" : "turned off");
                              }];
    }
}

Tray::~Tray() {
    System& s = *system_;
    [[NSDistributedNotificationCenter defaultCenter] removeObserver:s.helper];
    [[NSAppleEventManager sharedAppleEventManager] removeEventHandlerForEventClass:'aevt' andEventID:'rapp'];
    if (s.item) [[NSStatusBar systemStatusBar] removeStatusItem:s.item];
    if (s.notifications) [UNUserNotificationCenter currentNotificationCenter].delegate = nil;
}

uint32_t eventOf(const Tray::System& s) { return s.event; }

// Out of full screen first, as Messages does, so no empty Space is left behind. The app goes back to
// full screen when the window is shown again. The first time, a notification says where it went.
void hide(Tray::System& s, SDL_Window* window, const Asks& asks) {
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) {
        SDL_SetWindowFullscreen(window, false);
        SDL_SyncWindow(window);
    }
    SDL_HideWindow(window);
    dockIcon(false);
    if (s.notifications && allowedNow() && asks.sayStillRunning && asks.sayStillRunning())
        say(s, @"still-running", @"", @"Still running in the menu bar", nil);
}

void showing(Tray::System&) {
    dockIcon(true);
    [NSApp activate];
}

bool notify(Tray::System& s, const Invite& invite) {
    if (!s.notifications || !allowedNow()) {
        {
            std::lock_guard<std::mutex> lock(s.mu);
            s.bounced = invite.who;
        }
        [NSApp requestUserAttention:NSInformationalRequest];
        return false;
    }
    say(s, [@"invite-" stringByAppendingString:text(invite.who)], text(invite.title), text(invite.game),
        text(invite.who));
    return true;
}

void withdraw(Tray::System& s, const std::string& who) {
    {
        std::lock_guard<std::mutex> lock(s.mu);
        if (s.bounced == who) s.bounced.clear();
    }
    if (!s.notifications) return;
    NSArray* name = @[ [@"invite-" stringByAppendingString:text(who)] ];
    UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
    [center removeDeliveredNotificationsWithIdentifiers:name];
    [center removePendingNotificationRequestsWithIdentifiers:name];
}

void look(Tray::System& s, Status status, const std::string& tip) {
    if (!s.item) return;
    NSImage* image = pictureFor(s, status);
    if (image) {
        s.item.button.image = image;
        s.item.button.title = @"";
    } else {
        s.item.button.title = @"2P";
    }
    s.item.button.toolTip = text(tip);
}

Reply takeReply(Tray::System& s) {
    std::lock_guard<std::mutex> lock(s.mu);
    Reply r;
    std::swap(r, s.reply);
    return r;
}

}  // namespace tray
