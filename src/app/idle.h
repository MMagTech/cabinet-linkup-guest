// Whether anybody is at the computer, by the console's rule (linkupStatusWord in CabinetOS's
// linkupsession.cpp): away after kAwaySeconds with no key, mouse or controller, and at once while
// the screen is locked or dark or the computer sleeps. Back at the next touch. Friends then see
// Offline, as they would see a console left alone.
//
// The keys and the mouse are counted anywhere on the computer, in whichever program they land: the
// computer counts them itself. A controller it does not count, so the app adds its own presses
// (pressed), which SDL hands it from any program. In a game they are counted too, but only the window
// in front sends them to the host (sendPad in session.cpp).

#pragma once

#include <chrono>
#include <functional>
#include <memory>

namespace idle {

constexpr double kAwaySeconds = 15 * 60.0;   // linkupsession.cpp
constexpr int kAxisDeadzone = 12000;         // idle.h on the console, of 32767: a stick at rest moves less

// What the computer says, at one look.
struct Seen {
    double since = -1.0;   // seconds since the last key or mouse anywhere; negative when it cannot say
    bool off = false;      // the screen locked or dark, or the computer going to sleep
};

// The rule alone, with no computer in it. Each look gives what the computer says and the seconds since
// the app's own last press. While the screen is off, and after it, until somebody touches something,
// it is away: a screen that comes back by itself (a lid opened, the computer waking on its own) is
// nobody being there.
class Rule {
public:
    bool away(const Seen& seen, double sincePress);

private:
    double lastIdle_ = -1.0;
    bool held_ = false;      // was off, and nobody has touched anything since it went off
    bool touched_ = false;   // touched since it went off
};

// The rule, kept up to date from the computer. One for the app, on the window's thread.
class Watch {
public:
    // `wake` is called, from any thread, when the computer is about to sleep or the screen goes dark,
    // so the window stops waiting and the status goes before the network does.
    explicit Watch(std::function<void()> wake);
    ~Watch();
    Watch(const Watch&) = delete;
    Watch& operator=(const Watch&) = delete;

    // A press the app saw itself: a key, the mouse, or a controller.
    void pressed();
    // Away now. Looks at the computer at most once a second, and at once when it is going to sleep.
    bool away();

    struct System;   // each computer's own (idle.cpp)

private:
    std::unique_ptr<System> system_;
    Rule rule_;
    std::chrono::steady_clock::time_point lastPress_, lastLook_;
    bool looked_ = false;
    bool away_ = false;
};

}  // namespace idle
