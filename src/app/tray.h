// The window's comings and goings, each system's own way (MMagTech's choice, 2026-11-11), so the app
// keeps running and invites still reach the person with no window open:
//
//   Windows   the X hides the window and the app stays in the tray; the first time, one notification
//             says so. A click on the tray icon opens it; its right-click menu is Open, Status, Quit.
//             Minimize is the taskbar's, as ever.
//   macOS     the red button closes the window and the app stays in the Dock; the Dock icon brings
//             it back; Command-Q quits. No menu bar icon.
//   Both      an invite while the window is not in front is a system notification, and clicking it
//             opens Join or Decline. On a Mac that needs the app to be a .app (piece F): until then
//             the Dock icon bounces instead, and opens it.
//
// One copy runs for each folder: opening the app again while it runs brings back the one running.
//
// Everything here runs on the window's thread. What the system says comes back as SDL events, which
// take() turns into the asks below.

#pragma once

#include <functional>
#include <memory>
#include <string>

#include <SDL3/SDL.h>

namespace tray {

// Another copy already runs with `folder`: it is told to show its window, unless `quietly`, and this
// one should end. Called before SDL starts. False when this is the only one, which it stays: the
// folder is held until the program ends.
bool another(const std::string& folder, bool quietly);

// What the tray asks of the app.
struct Asks {
    std::function<std::string()> statusLine;           // the Status line as it reads now
    std::function<void()> flipStatus;                   // and pressed
    std::function<void(const std::string&)> answer;     // Join or Decline for this friend's invite
    std::function<bool()> sayStillRunning;              // true once ever, the first time the X hides it
};

class Tray {
public:
    Tray(SDL_Window* window, const std::string& folder, Asks asks);
    ~Tray();
    Tray(const Tray&) = delete;
    Tray& operator=(const Tray&) = delete;

    // An event that is the tray's: the window's close button, or the system's words for it. False for
    // any other, which goes on to the app.
    bool take(const SDL_Event& e);
    // The window, back and in front.
    void open();
    // Somebody invited this one while nobody may be looking: `title` is the console's line ("Invite
    // from Riley"), `game` under it. Said only while the window is not in front.
    void invited(const std::string& who, const std::string& title, const std::string& game);
    // The window is hidden, minimized, or behind another program's.
    bool hiddenAway() const;

    struct System;   // each system's own (tray_mac.mm, tray_win.cpp, tray_none.cpp)

private:
    SDL_Window* window_;
    Asks asks_;
    std::unique_ptr<System> system_;
};

}  // namespace tray
