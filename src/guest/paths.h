// Where 2P Start keeps what it holds: its key, the server it paired with and who it is there.
// One folder for each person on the computer, and nothing else in the program writes to disk.

#pragma once

#include <string>

namespace guest {

// Windows: "CabinetOS 2P Start" under the person's local AppData. macOS: the same name under their
// Library's Application Support. Elsewhere: cabinetos-2p-start under XDG_DATA_HOME, or under
// .local/share at home. Empty when the computer names no such place.
std::string dataFolder();

}  // namespace guest
