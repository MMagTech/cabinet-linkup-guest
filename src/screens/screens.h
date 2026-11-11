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
#include "ui/parts.h"

namespace screens {

// The destinations along the top.
enum class Bar { LinkUp, Settings };
void drawBar(ui::Canvas& c, Bar selected, bool focused = false);

// The friends list: Status, then a tile for each friend. `focus` is a tile, counted as drawn, or
// -1 for Status. With no friends, or with `state.instead` set, there is nothing to focus.
struct FriendsView {
    linkup::State state;
    int focus = 0;
    bool barFocused = false;   // focus is up in the top bar, and nothing below shows it
};
void drawFriends(ui::Canvas& c, const FriendsView& v);
// Which friend each tile is, in the order drawn: invites, then who can be invited, then who is busy,
// then who is away (linkup::groups).
std::vector<int> tileOrder(const std::vector<linkup::Friend>& friends);
// The colour of an invite's dot: the friend's own, brighter (LinkUpScreen::dotColour).
ui::Color friendDot(const std::vector<linkup::Friend>& friends, const std::string& name);

// A session with no picture to show: waiting for the host, the host paused, or trying again. One
// prompt, "Press (B) to leave", as the console's LinkUpPlayScreen has it: where the host paused, A
// resumes too, and nothing says so.
struct WaitingView {
    std::string title;     // "Waiting for Riley"
    std::string game;      // under it, when one is known
    parts::Hands hands = parts::Hands::Controller;
    float clock = 0.0f;
};
void drawWaiting(ui::Canvas& c, const WaitingView& v);

// Stands in for the game's picture, where there is no stream to show.
void drawPictureStandIn(ui::Canvas& c, const std::string& game, float clock);

// First run: the address, the code and a name, then Connect. Whatever is typed goes into the
// focused line, at its caret.
struct FirstRunView {
    std::string address, code, name;
    int focus = 0;         // 0 to 2 the lines, 3 Connect
    size_t caret = std::string::npos;   // in the focused line, in bytes; past the end is the end
    bool connecting = false;
    float clock = 0.0f;
    float still = 0.0f;    // seconds since the last key, for the caret
};
void drawFirstRun(ui::Canvas& c, const FirstRunView& v);

// Settings: what a friend can change. The colours are the console's own.
struct SettingsView {
    std::string name;          // as friends see you
    bool typingName = false;   // the name is being typed: a caret in it
    size_t caret = std::string::npos;   // in bytes; past the end is the end
    float still = 0.0f;        // seconds since the last key, for the caret
    int colour = 0;            // one of look's sixteen
    bool fullScreen = true;
    bool startup = false;      // the app opens at login, with no window
    std::vector<std::string> controllers;   // the ones connected, by name
    int controller = 0;                     // which of them plays
    // Its address, as the console's row shows it. Pressed, the row opens Change server and Leave.
    std::string server;
    int focus = 0;
    bool barFocused = false;
};
// The rows, in order: what `focus` counts.
namespace setting {
enum Row : int { Name, Colour, FullScreen, Startup, Controller, Server, About, Count };
}
void drawSettings(ui::Canvas& c, const SettingsView& v);

// About, as the console's Settings has it: the version, How to, and Credits and licenses. A list
// over Settings; `focus` is a line.
struct AboutView {
    std::string version;
    int focus = 0;
};
parts::Panel aboutPanel(const AboutView& v);
void drawAbout(ui::Canvas& c, const AboutView& v);

// Credits and licenses: a list over Settings, one line for each project whose work is in the app,
// what it does and its licence (credits.h). `focus` is a line, `top` the first shown.
struct CreditsView {
    int focus = 0;
    int top = 0;
};
int creditCount();
void drawCredits(ui::Canvas& c, const CreditsView& v);

// How to: what playing needs, in the console's own lines (about.cpp, "In a game" and "Menus"), only
// those that are a friend's. A list over Settings with nothing to press; `focus` is a line.
struct HowToView {
    int focus = 0;
    int top = 0;
};
int howToCount();
void drawHowTo(ui::Canvas& c, const HowToView& v);

}  // namespace screens
