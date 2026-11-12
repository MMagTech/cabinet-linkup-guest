// The window's comings and goings, each system's own way (MMagTech's choice, 2026-11-11), so the app
// keeps running and invites still reach the person with no window open:
//
//   Windows   an icon in the tray: a click opens the window, and a right-click opens its menu. The X
//             hides the window; the first time, a notification says it is still running.
//   macOS     an icon in the menu bar, whose click opens the same menu. The red button closes the
//             window, and the Dock icon is there only while the window is open; Command-Q quits.
//   Both      the menu: each invite waiting ("Invite from Riley: Burnout 3"), which opens Join or
//             Decline; Status, Online or Offline, ticked; Open 2P Start; Startup, ticked; Quit. The
//             icon shows the status, and its hover words say it ("2P Start: Online", "Invite from
//             Riley"). An invite while the window is not in front is a notification with Join and
//             Decline on it; a click on the rest of it opens the question.
//
// With Keep running off, the close button quits instead. One copy runs for each folder: opening the
// app again while it runs brings back the one running.
//
// Everything here runs on the window's thread. What the system says comes back as SDL events, which
// take() turns into the asks below.

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

namespace tray {

// Another copy already runs with `folder`: it is told to show its window, unless `quietly`, and this
// one should end. Called before SDL starts. False when this is the only one, which it stays: the
// folder is held until the program ends.
bool another(const std::string& folder, bool quietly);

// An invite waiting: who, the console's line for it ("Invite from Riley"), and the game.
struct Invite {
    std::string who;
    std::string title;
    std::string game;
};

// What the icon shows: the status friends see, or an invite waiting.
enum class Status { Offline, Online, Invite };

// What the tray asks of the app.
struct Asks {
    std::function<std::vector<Invite>()> invites;
    std::function<bool()> online;                         // the Status switch: Online, not Offline
    std::function<void(bool)> setOnline;
    // This friend's invite: -1 opens Join or Decline, 0 joins, 1 declines.
    std::function<void(const std::string&, int)> reply;
    std::function<bool()> startup;                        // Startup's row is on
    std::function<bool()> canStartup;                     // and can be changed: Keep running is on
    std::function<void()> flipStartup;
    std::function<bool()> keepRunning;                    // the close button hides, rather than quits
    std::function<bool()> sayStillRunning;                // true once ever, the first time it hides
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
    // Somebody invited this one while nobody may be looking. Said by the system, with its own sound,
    // only while the window is not in front: true when it was.
    bool invited(const Invite& invite);
    // The icon follows the invites waiting and whether friends see this one Online, and a
    // notification for an invite that has ended is taken back. Called on every pass of the loop: it
    // changes things only when they change.
    void follow(const std::vector<Invite>& waiting, bool online);
    // The window is hidden, minimized, or behind another program's.
    bool hiddenAway() const;

    struct System;   // each system's own (tray_mac.mm, tray_win.cpp, tray_none.cpp)

private:
    SDL_Window* window_;
    Asks asks_;
    std::unique_ptr<System> system_;
    Status shown_ = Status::Offline;
    std::string tip_;
    std::vector<std::string> waiting_;   // who has an invite waiting, as last followed
};

// The icon's status and hover words for what is waiting (Tray::follow), for the tests too.
Status statusOf(const std::vector<Invite>& waiting, bool online);
std::string tipOf(const std::vector<Invite>& waiting, bool online);
// The menu's line for an invite: "Invite from Riley: Burnout 3".
std::string lineOf(const Invite& invite);

}  // namespace tray
