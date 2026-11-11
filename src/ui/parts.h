// The parts more than one screen draws, each as the console draws it: the top bar, a line of
// words that takes focus, the press prompt, the notice at the foot, the panel that asks a question,
// a row, and the cabinet. Sizes and colours are the console's (bar.cpp, screenparts.cpp, notice.cpp,
// choice.cpp, panels.h, startscreen.cpp).

#pragma once

#include <string>
#include <vector>

#include "ui/draw.h"

namespace parts {

constexpr float kInset = 60.0f;          // the margin every screen starts on
constexpr float kBarTop = 44.0f;
constexpr float kBarHeight = 56.0f;
constexpr float kContentTop = kBarTop + kBarHeight + 68.0f;
constexpr float kFocusRim = 4.0f;
constexpr float kRowHeight = 92.0f;
constexpr float kRowGap = 14.0f;

// The destinations along the top. `selected` is the screen being shown; `focus` is 0 to 1 when
// the cursor is on it.
void topBar(ui::Canvas& c, const std::vector<std::string>& labels, int selected, float focus);

// A line of words on the margin. The capsule appears only under focus.
float pillHeight(ui::Canvas& c);
void pill(ui::Canvas& c, const std::string& label, float x, float y, bool selected, float focus);

// "Press (A) to resume", centred on `midX`, the button as a white disc with a dark letter.
void pressPrompt(ui::Canvas& c, const std::string& before, const std::string& button, const std::string& after,
                 float midX, float baseline, float alpha);

// The line at the foot of the screen that says what just happened. The dot says which kind.
enum class Tone { Info, Done, Problem };
void notice(ui::Canvas& c, const std::string& words, Tone tone);

// A panel over the screen: a question and its answers, or a line and a prompt.
struct Panel {
    std::string title;
    std::string detail;
    std::vector<std::string> options;
    int focused = 0;
    // With no options: "Press (A) to resume" in their place. Empty for none.
    std::string promptBefore, promptButton, promptAfter;
};
void panel(ui::Canvas& c, const Panel& p);

// A row as the console's menus draw one: a label, and what it is set to. `focus` is 0 to 1.
// `dim` is a value nobody has given yet.
void row(ui::Canvas& c, float x, float y, float w, const std::string& label, const std::string& value, float focus,
         bool dim = false);
// The same shape with one word in the middle: something to press.
void button(ui::Canvas& c, float x, float y, float w, const std::string& label, float focus);

// The console's cabinet, `h` tall. Its width is 527/720 of that.
void cabinet(ui::Canvas& c, float x, float y, float h);
// The same, standing back: there, but not lit.
void cabinetDim(ui::Canvas& c, float x, float y, float h);

// Three dots in turn: working, not stuck.
void waitingDots(ui::Canvas& c, float midX, float y, float clock);

}  // namespace parts
