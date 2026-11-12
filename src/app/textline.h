// What a line keeps of what is typed or pasted into it: an address, a pairing code or a name, where
// its caret is, and the letters chosen in it. As in the console's keyboard field (keyboard.cpp),
// everything goes in at the caret, which then sits after it; Backspace takes the letter before it
// and Delete the one after; and the caret moves a whole letter at a time, stopping at the ends.
//
// The console types only with a controller, so it chooses nothing. Here letters are chosen as in any
// app's text box, from an anchor to the caret: what is typed or pasted then takes their place, and
// Backspace or Delete takes them away. An arrow without Shift lets go of them, the caret at their
// end on its side.
//
// The code is tidied as the console tidies one (linkup::tidyCode): upper case, in groups of four,
// look-alikes dropped. Its dashes are the code's, not typed, so its caret counts letters: it is
// after the same letter once the code is grouped again, and never on the far side of a dash. A
// name is cleaned as the console cleans anything a person reads (box.h), and does not start with a
// space. A full line takes nothing more: what is after the caret is never pushed off its end.

#pragma once

#include <optional>
#include <string>
#include <utility>

namespace textline {

enum class Kind { Address, Code, Name };

// The words, and the caret in them in bytes, on the edge of a letter. The chosen letters are from
// `anchor` to the caret: none when there is no anchor.
struct Line {
    std::string text;
    size_t caret = 0;
    std::optional<size_t> anchor = std::nullopt;
};

// `text` typed or pasted in at the caret, in place of the chosen letters. When nothing of it is
// kept, a dash in a code or a space in an address, nothing changes.
Line add(Line now, const std::string& text, Kind kind);
// The chosen letters gone, or else the letter before the caret, however many bytes it is. In a
// code, the letter before a dash.
Line backspace(Line now, Kind kind);
// The chosen letters gone, or else the letter after the caret.
Line forwardDelete(Line now, Kind kind);
// The caret a letter left (`by` -1) or right (1), stopping at the ends. `choosing` (Shift) chooses
// the letters it passes, from where it was.
Line step(Line now, int by, Kind kind, bool choosing = false);
// The caret at `at`, or the nearest place to it a caret can be: for a click, Home and End. With
// `choosing`, a Shift-click or a drag, what is between it and the anchor is chosen.
Line moveTo(Line now, size_t at, Kind kind, bool choosing = false);
// The word at `at` chosen: a double-click. Letters, digits and _ make a word; anything else is
// chosen alone. In a code, a word is a group of four.
Line word(Line now, size_t at, Kind kind);
// Every letter chosen.
Line all(Line now, Kind kind);
// Where the chosen letters are, in bytes, first and past last: the same when none are. A code's
// start on its first letter, not a dash before it.
std::pair<size_t, size_t> chosen(Line now, Kind kind);
// The chosen letters, for Copy and Cut.
std::string chosenText(Line now, Kind kind);
// Whether it is filled in enough to send.
bool ready(const std::string& value, Kind kind);

// The longest address kept, in bytes: a host name's most, and a port.
constexpr size_t kMaxAddress = 260;

}  // namespace textline
