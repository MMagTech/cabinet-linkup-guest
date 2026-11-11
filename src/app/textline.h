// What a line keeps of what is typed or pasted into it: an address, a pairing code or a name.
// The code is tidied as the console tidies one (linkup::tidyCode): upper case, in groups of four,
// look-alikes dropped. A name is cleaned as the console cleans anything a person reads (box.h).

#pragma once

#include <string>

namespace textline {

enum class Kind { Address, Code, Name };

// `now` with `text` added at its end, as typed or pasted.
std::string add(const std::string& now, const std::string& text, Kind kind);
// `now` with its last letter gone, however many bytes the letter is.
std::string backspace(const std::string& now, Kind kind);
// Whether it is filled in enough to send.
bool ready(const std::string& value, Kind kind);

// The longest address kept, in bytes: a host name's most, and a port.
constexpr size_t kMaxAddress = 260;

}  // namespace textline
