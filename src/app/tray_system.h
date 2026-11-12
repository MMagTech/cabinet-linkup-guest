// What each system does for the tray (tray.h): tray_mac.mm, tray_win.cpp and tray_none.cpp each
// define Tray::System, the Tray's constructor and destructor, and these. tray.cpp is the part they share.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "app/tray.h"

namespace tray {

// The codes of the tray's own SDL event: what the system asked for.
enum class Ask : int32_t { Open = 1, Online, Offline, Reply, Startup };

// Asks it, from any thread.
void push(uint32_t event, Ask ask);

// An invite answered from the menu or a notification: who, and -1 (the question), 0 (Join) or 1
// (Decline), as Asks::reply takes them.
struct Reply {
    std::string who;
    int answer = -1;
};

// The tray's own SDL event type.
uint32_t eventOf(const Tray::System& s);
// The window's close button, with Keep running on: the window hides, and the app keeps running.
void hide(Tray::System& s, SDL_Window* window, const Asks& asks);
// The window comes back: on a Mac, with its Dock icon.
void showing(Tray::System& s);
// A system notification for an invite: see Tray::invited. False when the system shows none, as when
// the person said no to its notifications.
bool notify(Tray::System& s, const Invite& invite);
// The notification for this friend's invite taken back: it has ended.
void withdraw(Tray::System& s, const std::string& who);
// The icon's picture and hover words.
void look(Tray::System& s, Status status, const std::string& tip);
// The reply a click asked for, once: empty `who` when none did.
Reply takeReply(Tray::System& s);

// A short name for `folder`, the same each time: what one copy and the next both call it.
std::string folderKey(const std::string& folder);

}  // namespace tray
