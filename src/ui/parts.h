// The parts more than one screen draws, each as the console draws it: the top bar, a line of
// words that takes focus, the press prompt, the notice at the foot, the panel that asks a question,
// a row, and the cabinet. Sizes and colours are the console's (bar.cpp, screenparts.cpp, notice.cpp,
// choice.cpp, panels.h, settings.cpp, startscreen.cpp).

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

// What a pointer can land on (ui::Spot): each part records its own as it draws it.
namespace spot {
constexpr int kBar = 1000;       // + the destination
constexpr int kStatus = 2000;    // the Status line
constexpr int kItem = 3000;      // + a tile, a row, or first run's lines and Connect
constexpr int kOption = 4000;    // + a panel's answer or line
constexpr int kField = 5000;     // a line being typed in a panel
constexpr int kPanel = 6000;     // a panel itself: a click on it is not a click outside it
}  // namespace spot

// What the person last touched, so a prompt names what they have in their hands.
enum class Hands { Controller, Keyboard, Mouse };

// "Press (A) to resume": the words before, the button, the words after. A mouse has no button to
// name: "Click to resume".
struct Prompt {
    std::string before, button, after;
};
// A for going on, B for going back, in the words for those hands.
Prompt prompt(Hands hands, bool back, const std::string& after);

// A prompt centred on `midX`. A one-letter button is a white disc with a dark letter, a key's name
// a white capsule, and no button is the words alone.
void pressPrompt(ui::Canvas& c, const Prompt& p, float midX, float baseline, float alpha);

// The line at the foot of the screen that says what just happened. The dot says which kind; an
// invite's is the friend's own colour, `dot`.
enum class Tone { Info, Done, Busy, Problem };
void notice(ui::Canvas& c, const std::string& words, Tone tone, const ui::Color* dot = nullptr);

// A panel over the screen: a question and its answers, a list, or a line and a prompt
// (choice.cpp on the console).
constexpr int kPanelRows = 6;              // more than this scrolls
constexpr float kLinkUpPanelWidth = 780.0f;
struct Panel {
    std::string title;
    std::string detail;
    std::vector<std::string> options;
    // A list: each option with its value on the right, "name ... value", like a Settings row.
    std::vector<std::string> values;
    int focused = 0;
    int top = 0;          // the first row shown, when there are more than kPanelRows
    // Every LinkUp question is one width, kLinkUpPanelWidth, and a long title goes on two lines
    // rather than being cut. 0: the panel follows its longest line.
    float width = 0.0f;
    // With no options: "Press (A) to resume" in their place. Empty for none.
    Prompt prompt;
};
void panel(ui::Canvas& c, const Panel& p);

// A row as the console's Settings draws one: a label, and what it is set to.
struct Row {
    std::string label;
    std::string value;
    bool dim = false;        // a value nobody has given yet
    bool opens = false;      // it opens something: a chevron at the right
    // One of a few, which Left and Right move: under focus, an arrow either side of the value,
    // faint at the end it cannot pass.
    bool choice = false;
    bool canLeft = true, canRight = true;
    // Being typed into: the caret at `caret`, in bytes (past the end is the end), and the part of
    // the line with the caret in it kept in view.
    bool typing = false;
    size_t caret = std::string::npos;
    float still = 0.0f;      // seconds since the last key, for the caret's blink
    // A line that is typed into, whether it is now or not: where its letters fall is kept under
    // this spot, for a click to put the caret between two (Canvas::letters). 0 for any other row.
    int line = 0;
};
// `focus` is 0 to 1.
void row(ui::Canvas& c, float x, float y, float w, const Row& r, float focus);

// The console's caret (keyboard.cpp): cyan, on the letters, at `x`, on the gap between two of them
// and drawn over them, so the letters do not move as it walks through. It holds still for half a
// second after a key and then blinks once a second, fading.
void caret(ui::Canvas& c, float x, float baseline, ui::TextStyle st, float still);

// A line to type into, as the console's on-screen keyboard shows one, with no keys under it: a
// computer has its own. The title, a hint under it, the field, and what Enter does.
struct FieldPanel {
    std::string title;
    std::string hint;          // which server a code is for
    std::string value;
    size_t caret = std::string::npos;   // in bytes; past the end is the end
    std::string placeholder;
    float still = 0.0f;
    Prompt prompt;
};
void fieldPanel(ui::Canvas& c, const FieldPanel& p);
// The same shape with one word in the middle: something to press.
void button(ui::Canvas& c, float x, float y, float w, const std::string& label, float focus);

// The console's cabinet, `h` tall. Its width is 527/720 of that.
void cabinet(ui::Canvas& c, float x, float y, float h);
// The same, standing back: there, but not lit.
void cabinetDim(ui::Canvas& c, float x, float y, float h);

// Three dots in turn: working, not stuck.
void waitingDots(ui::Canvas& c, float midX, float y, float clock);

}  // namespace parts
