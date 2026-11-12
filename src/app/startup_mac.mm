// Startup inside the Mac app (startup.h): the app's own Login Item, by the launch agent it carries in
// Contents/Library/LaunchAgents, which starts it with --startup.

#include "app/startup.h"

#import <Foundation/Foundation.h>
#import <ServiceManagement/ServiceManagement.h>

namespace startup {

bool inApp() {
    NSBundle* b = [NSBundle mainBundle];
    return b.bundleIdentifier != nil && [b.bundleURL.pathExtension isEqualToString:@"app"];
}

bool setLoginItem(bool on, std::string& why) {
    if (@available(macOS 13.0, *)) {
        SMAppService* item = [SMAppService agentServiceWithPlistName:[NSString stringWithFormat:@"%s.plist", kLabel]];
        const SMAppServiceStatus now = item.status;
        if (on == (now == SMAppServiceStatusEnabled)) return true;
        if (!on && now != SMAppServiceStatusEnabled && now != SMAppServiceStatusRequiresApproval) return true;
        NSError* e = nil;
        const BOOL done = on ? [item registerAndReturnError:&e] : [item unregisterAndReturnError:&e];
        if (!done) why = e ? std::string(e.localizedDescription.UTF8String) : std::string("Login Items said no");
        return done;
    }
    why = "Startup needs macOS 13 or later";
    return false;
}

}  // namespace startup
