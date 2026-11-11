// What a line keeps of what is typed or pasted into it: an address, a pairing code or a name, and
// where its caret is. As in the console's keyboard field (keyboard.cpp), everything goes in at the
// caret, which then sits after it; Backspace takes the letter before it and Delete the one after;
// and the caret moves a whole letter at a time, stopping at the ends.
//
// The code is tidied as the console tidies one (linkup::tidyCode): upper case, in groups of four,
// look-alikes dropped. Its dashes are the code's, not typed, so its caret counts letters: it is
// after the same letter once the code is grouped again, and never on the far side of a dash. A
// name is cleaned as the console cleans anything a person reads (box.h), and does not start with a
// space. A full line takes nothing more: what is after the caret is never pushed off its end.

#pragma once

#include <string>

namespace textline {

enum class Kind { Address, Code, Name };

// The words, and the caret in them in bytes, on the edge of a letter.
struct Line {
    std::string text;
    size_t caret = 0;
};

// `text` typed or pasted in at the caret.
Line add(Line now, const std::string& text, Kind kind);
// The letter before the caret gone, however many bytes it is. In a code, the letter before a dash.
Line backspace(Line now, Kind kind);
// The letter after the caret gone.
Line forwardDelete(Line now, Kind kind);
// The caret a letter left (`by` -1) or right (1), stopping at the ends.
Line step(Line now, int by, Kind kind);
// The caret at `at`, or the nearest place to it a caret can be: for a click, Home and End.
Line moveTo(Line now, size_t at, Kind kind);
// Whether it is filled in enough to send.
bool ready(const std::string& value, Kind kind);

// The longest address kept, in bytes: a host name's most, and a port.
constexpr size_t kMaxAddress = 260;

}  // namespace textline
