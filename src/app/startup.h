// Startup, the row in Settings: the app opens when the person logs in, with no window (--startup),
// so invites reach them with nothing to remember. Off at first; what the row says is kept in
// settings.json, and the computer's own entry follows it.
//
// The entry is the computer's way for a program that is not yet packaged, and starts this program
// where it is now:
//   macOS     a launch agent, ~/Library/LaunchAgents/io.github.mmagtech.2pstart.plist
//   Windows   a value under HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run
// Once 2P Start is packaged (piece F), each moves to the package's own: on a Mac, SMAppService for
// the .app (System Settings lists it under Open at Login by its name); on Windows, the StartupTask in
// the MSIX's manifest, with the zip keeping the Run value. The app then removes its old entry.

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

}  // namespace startup
