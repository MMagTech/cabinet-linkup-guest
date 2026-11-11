// What a line keeps (textline.h).

#include "app/textline.h"

#include "box.h"
#include "guest/account.h"
#include "linkuprules.h"

namespace textline {

namespace {

// The bytes of the last whole letter of `s`, counted back from its end.
size_t lastLetter(const std::string& s) {
    size_t n = 0;
    while (n < s.size()) {
        ++n;
        const unsigned char c = static_cast<unsigned char>(s[s.size() - n]);
        if ((c & 0xC0) != 0x80) break;
    }
    return n;
}

}  // namespace

std::string add(const std::string& now, const std::string& text, Kind kind) {
    switch (kind) {
        case Kind::Code: return linkup::tidyCode(now + text);
        case Kind::Name: {
            std::string out = box::cleanText(now + text, box::kMaxUsername);
            // A name does not start with a space.
            const size_t first = out.find_first_not_of(' ');
            return first == std::string::npos ? std::string() : out.substr(first);
        }
        case Kind::Address: {
            // No spaces or line ends in an address: a pasted one often brings some.
            std::string out = now;
            for (unsigned char c : text) {
                if (c <= 0x20 || c == 0x7F) continue;
                out.push_back(static_cast<char>(c));
            }
            out = box::cleanText(out, kMaxAddress);
            return out;
        }
    }
    return now;
}

std::string backspace(const std::string& now, Kind kind) {
    if (now.empty()) return now;
    if (kind == Kind::Code) {
        // The dash between groups is the code's, not typed: the letter before it goes.
        std::string bare;
        for (char c : now)
            if (c != '-') bare.push_back(c);
        bare.pop_back();
        return linkup::tidyCode(bare);
    }
    return now.substr(0, now.size() - lastLetter(now));
}

bool ready(const std::string& value, Kind kind) {
    switch (kind) {
        case Kind::Code: return linkup::codeComplete(value);
        case Kind::Name: {
            const size_t last = value.find_last_not_of(' ');
            return last != std::string::npos && guest::usernameFits(value.substr(0, last + 1));
        }
        case Kind::Address: return !value.empty();
    }
    return false;
}

}  // namespace textline
