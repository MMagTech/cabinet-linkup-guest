// What a line keeps (textline.h).

#include "app/textline.h"

#include <algorithm>
#include <cstddef>

#include "box.h"
#include "guest/account.h"
#include "linkuprules.h"

namespace textline {

namespace {

// A byte after the first of a letter (keyboard.cpp, continuationByte).
bool insideLetter(char c) { return (static_cast<unsigned char>(c) & 0xC0) == 0x80; }

size_t previousLetter(const std::string& s, size_t at) {
    if (at == 0) return 0;
    --at;
    while (at > 0 && insideLetter(s[at])) --at;
    return at;
}

size_t nextLetter(const std::string& s, size_t at) {
    if (at >= s.size()) return s.size();
    ++at;
    while (at < s.size() && insideLetter(s[at])) ++at;
    return at;
}

std::string lettersOf(const std::string& code) {
    std::string out;
    for (char c : code)
        if (c != '-') out.push_back(c);
    return out;
}

size_t lettersBefore(const std::string& code, size_t at) {
    return static_cast<size_t>(std::count_if(code.begin(), code.begin() + static_cast<std::ptrdiff_t>(std::min(at, code.size())),
                                             [](char c) { return c != '-'; }));
}

// Where the caret after `k` letters of a code is: straight after the letter, before any dash.
size_t afterLetters(const std::string& code, size_t k) {
    if (k == 0) return 0;
    size_t seen = 0;
    for (size_t i = 0; i < code.size(); ++i)
        if (code[i] != '-' && ++seen == k) return i + 1;
    return code.size();
}

// The most letters a code has, as tidyCode keeps them: asked of it, so the rule stays the console's.
size_t codeLetters() {
    static const size_t n = lettersOf(linkup::tidyCode(std::string(64, 'A'))).size();
    return n;
}

// A code from its letters, grouped, with the caret after `k` of them.
Line grouped(const std::string& letters, size_t k) {
    Line out;
    out.text = linkup::tidyCode(letters);
    out.caret = afterLetters(out.text, k);
    return out;
}

// A name does not start with a space: any left at its start go, and the caret moves back with them.
Line noSpaceFirst(Line l) {
    const size_t first = l.text.find_first_not_of(' ');
    const size_t n = first == std::string::npos ? l.text.size() : first;
    l.text.erase(0, n);
    l.caret = l.caret > n ? l.caret - n : 0;
    return l;
}

// The caret no further than the end, on the edge of a letter, and in a code just after a letter.
Line placed(Line l, Kind kind) {
    l.caret = std::min(l.caret, l.text.size());
    while (l.caret > 0 && l.caret < l.text.size() && insideLetter(l.text[l.caret])) --l.caret;
    if (kind == Kind::Code) l.caret = afterLetters(l.text, lettersBefore(l.text, l.caret));
    return l;
}

size_t roomIn(const std::string& s, size_t most) { return most > s.size() ? most - s.size() : 0; }

}  // namespace

Line add(Line now, const std::string& text, Kind kind) {
    now = placed(std::move(now), kind);
    switch (kind) {
        case Kind::Code: {
            std::string letters = lettersOf(now.text);
            const size_t k = lettersBefore(now.text, now.caret);
            std::string typed = lettersOf(linkup::tidyCode(text));
            typed.resize(std::min(typed.size(), roomIn(letters, codeLetters())));
            letters.insert(k, typed);
            return grouped(letters, k + typed.size());
        }
        case Kind::Name: {
            const std::string typed = box::cleanText(text, roomIn(now.text, box::kMaxUsername));
            now.text.insert(now.caret, typed);
            now.caret += typed.size();
            return noSpaceFirst(std::move(now));
        }
        case Kind::Address: {
            // No spaces or line ends in an address: a pasted one often brings some.
            std::string kept;
            for (unsigned char c : text) {
                if (c <= 0x20 || c == 0x7F) continue;
                kept.push_back(static_cast<char>(c));
            }
            const std::string typed = box::cleanText(kept, roomIn(now.text, kMaxAddress));
            now.text.insert(now.caret, typed);
            now.caret += typed.size();
            return now;
        }
    }
    return now;
}

Line backspace(Line now, Kind kind) {
    now = placed(std::move(now), kind);
    if (now.caret == 0) return now;
    if (kind == Kind::Code) {
        // The dash between groups is the code's, not typed: the letter before it goes.
        std::string letters = lettersOf(now.text);
        const size_t k = lettersBefore(now.text, now.caret);
        letters.erase(k - 1, 1);
        return grouped(letters, k - 1);
    }
    const size_t from = previousLetter(now.text, now.caret);
    now.text.erase(from, now.caret - from);
    now.caret = from;
    return kind == Kind::Name ? noSpaceFirst(std::move(now)) : now;
}

Line forwardDelete(Line now, Kind kind) {
    now = placed(std::move(now), kind);
    if (now.caret >= now.text.size()) return now;
    if (kind == Kind::Code) {
        std::string letters = lettersOf(now.text);
        const size_t k = lettersBefore(now.text, now.caret);
        letters.erase(k, 1);
        return grouped(letters, k);
    }
    now.text.erase(now.caret, nextLetter(now.text, now.caret) - now.caret);
    return kind == Kind::Name ? noSpaceFirst(std::move(now)) : now;
}

Line step(Line now, int by, Kind kind) {
    now = placed(std::move(now), kind);
    if (kind == Kind::Code) {
        const size_t k = lettersBefore(now.text, now.caret);
        const size_t n = lettersOf(now.text).size();
        now.caret = afterLetters(now.text, by < 0 ? (k > 0 ? k - 1 : 0) : std::min(k + 1, n));
        return now;
    }
    now.caret = by < 0 ? previousLetter(now.text, now.caret) : nextLetter(now.text, now.caret);
    return now;
}

Line moveTo(Line now, size_t at, Kind kind) {
    now.caret = at;
    return placed(std::move(now), kind);
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
