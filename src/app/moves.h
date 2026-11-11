// Where focus goes, for each move, on each kind of screen: the console's rules (LinkUpScreen::key in
// linkupscreen.cpp, barKey in bar.cpp, the choice panel in choice.cpp), with nothing drawn and
// nothing pressed. The app does what a step says.

#pragma once

namespace moves {

// What a controller, the keys or the mouse asked for. A press is A, Enter or a click; Back is B, Esc
// or a right-click. The shoulders change screen.
enum class Move { Up, Down, Left, Right, Press, Back, PrevScreen, NextScreen };

struct Step {
    enum class Then {
        Moved,     // focus is now at `focus`
        Edge,      // nowhere to go: focus stays
        Press,     // the focused thing is pressed
        Back,      // go back, or cancel
        ToBar,     // up into the top bar
    };
    int focus = 0;
    Then then = Then::Edge;
};

// The friends list: -1 is the Status line, then the tiles, kColumns to a row.
constexpr int kColumns = 4;
Step friends(int focus, int tiles, Move m);

// A column of rows, top to bottom. Up from the top goes into the top bar when there is one.
Step column(int focus, int count, Move m, bool bar);

// The top bar's two places. Left and Right switch the screen, as the console's do; Down, A and Back
// all go back down into the screen, when it has something to focus.
struct BarStep {
    int at = 0;
    bool leave = false;    // focus goes back into the screen
    bool edge = false;
};
BarStep bar(int at, int count, Move m, bool screenTakesFocus);

// The first row a list of `rows` shows so that `focus` is among them, moving as little as it can.
int topFor(int focus, int top, int count, int rows);

}  // namespace moves
