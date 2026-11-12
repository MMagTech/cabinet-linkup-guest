// What a friend changed in Settings, kept in the app's folder so it is still there next time:
// the colour, full screen, Keep running, Startup, which controller plays, and Appear Offline. The name is kept
// on its own beside the key (account.h), because the server knows the friend by it.

#pragma once

#include <string>

namespace guest {

struct Settings {
    std::string colour = "purple";    // one of the console's sixteen, by its word (look.h)
    bool fullScreen = false;
    std::string controller;           // the one that plays, by SDL's GUID; empty for the first there
    bool appearOffline = false;
    bool keepRunning = true;          // the close button hides the window; Off, it quits (app/tray.h)
    bool startup = false;             // opens at login, with no window (app/startup.h), while it keeps running
    bool saidTray = false;            // Windows said once that the X keeps it running in the tray
    std::string sounds = "medium";    // Interface sounds, by the console's word: off, quiet, medium, loud
};

// What is kept, or the defaults for anything missing or unreadable.
Settings readSettings(const std::string& folder);
bool keepSettings(const std::string& folder, const Settings& s);

}  // namespace guest
