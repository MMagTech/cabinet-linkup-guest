// What each system does for the tray (tray.h): tray_mac.mm, tray_win.cpp and tray_none.cpp each
// define Tray::System, the Tray's constructor and destructor, and these. tray.cpp is the part they share.

#pragma once

#include <cstdint>
#include <string>

#include "app/tray.h"

namespace tray {

// The codes of the tray's own SDL event: what the system asked for.
enum class Ask : int32_t { Open = 1, Status, Answer };

// Asks it, from any thread.
void push(uint32_t event, Ask ask);

// The tray's own SDL event type.
uint32_t eventOf(const Tray::System& s);
// The window's close button: it hides, and the app keeps running.
void hide(Tray::System& s, SDL_Window* window, const Asks& asks);
// A system notification for an invite: see Tray::invited.
void notify(Tray::System& s, const std::string& who, const std::string& title, const std::string& game);
// The friend a clicked notification named, once: empty when none did.
std::string takeAnswer(Tray::System& s);

// A short name for `folder`, the same each time: what one copy and the next both call it.
std::string folderKey(const std::string& folder);

}  // namespace tray
