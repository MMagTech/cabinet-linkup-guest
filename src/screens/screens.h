// 2P Start's screens. Each takes what to show and draws it; none of them knows a server, a stream
// or a controller. They are LinkUp's screens on the console, as a friend sees them
// (frontend/src/linkupscreen.cpp in CabinetOS), with the same numbers and the console's own words
// (linkuprules.h, built from the submodule). First run is the one screen the console does not
// have in this form: it pairs from its Settings.

#pragma once

#include <string>
#include <vector>

#include "linkuprules.h"
#include "ui/draw.h"

namespace screens {

// The destinations along the top.
enum class Bar { LinkUp, Settings };
void drawBar(ui::Canvas& c, Bar selected, bool focused = false);

// The friends list: Status, then a tile for each friend. `focus` is a tile, counted as drawn, or
// -1 for Status. With no friends, or with `state.instead` set, there is nothing to focus.
struct FriendsView {
    linkup::State state;
    int focus = 0;
};
void drawFriends(ui::Canvas& c, const FriendsView& v);

// A session with no picture to show: waiting for the host, the host paused, or trying again.
struct WaitingView {
    std::string title;     // "Waiting for Riley"
    std::string game;      // under it, when one is known
    // "Press (A) to resume" above "Press (B) to leave", when the host paused and either may resume.
    bool resumes = false;
    float clock = 0.0f;
};
void drawWaiting(ui::Canvas& c, const WaitingView& v);

// Stands in for the game's picture, where there is no stream to show.
void drawPictureStandIn(ui::Canvas& c, const std::string& game, float clock);

// First run: the address, the code and a name, then Connect.
struct FirstRunView {
    std::string address, code, name;
    int focus = 0;         // 0 to 2 the lines, 3 Connect
    bool connecting = false;
    float clock = 0.0f;
};
void drawFirstRun(ui::Canvas& c, const FirstRunView& v);

// Settings: what a friend can change. The colours are the console's own.
struct SettingsView {
    std::string name;          // as friends see you
    std::string colour;        // "Purple"
    bool fullScreen = true;
    std::string controller;    // empty for none connected
    std::string server;
    int focus = 0;
};
void drawSettings(ui::Canvas& c, const SettingsView& v);

}  // namespace screens
