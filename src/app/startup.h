// Startup, the row in Settings: the app opens when the person logs in, with no window (--startup),
// so invites reach them with nothing to remember. Off at first; what the row says is kept in
// settings.json, and the computer's own entry follows it.
//
// The entry is each system's own, and starts this program where it is now:
//   macOS     inside the app, its own Login Item (SMAppService, with the launch agent the app carries,
//             cmake/launch-agent.plist), which System Settings lists by its name and which follows
//             the app wherever it is moved; run as a bare program, as while it is worked on, a launch
//             agent of its own, ~/Library/LaunchAgents/io.github.mmagtech.2pstart.plist
//   Windows   a value under HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run, written
//             again at each start, so it follows the program wherever it is
// The app inside the Mac app takes away the launch agent a bare copy wrote.

#pragma once

#include <string>

namespace startup {

constexpr const char* kFlag = "--startup";
constexpr const char* kLabel = "io.github.mmagtech.2pstart";   // the launch agent's, as the app's own id

// Sets the computer's entry on or off, for this program where it is now. Setting it on again moves it
// to here, should the program have moved. False, with why, when the computer would not.
bool set(bool on, std::string& why);

// The launch agent that starts `program` with --startup, for the test: an Apple property list.
std::string agent(const std::string& program);
// The same, written into `folder` as kLabel.plist, or removed from it.
bool setAgent(const std::string& folder, const std::string& program, bool on, std::string& why);
// The Run value that starts `program` with --startup, in UTF-8: the path quoted, as Windows wants it.
std::string runValue(const std::string& program);

// Inside the Mac app (startup_mac.mm): whether this is one, and its Login Item set on or off.
bool inApp();
bool setLoginItem(bool on, std::string& why);

}  // namespace startup
