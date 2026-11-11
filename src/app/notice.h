// The line at the foot of the screen, and how long it stays: the console's MenuNotice (notice.h in
// CabinetOS), with its numbers. parts::notice draws it.

#pragma once

#include <algorithm>
#include <optional>
#include <string>

#include "ui/draw.h"
#include "ui/parts.h"

struct Notice {
    std::string text;
    parts::Tone tone = parts::Tone::Info;
    float life = 0.0f;
    float age = 0.0f;
    // A line nobody may be there for: it stays until the next press, then has what is left of its
    // time, and goes soon if that has already passed.
    bool holdUntilPress = false;
    // An invite's dot is the friend's own colour.
    std::optional<ui::Color> dot;

    void pressed() {
        if (!holdUntilPress) return;
        holdUntilPress = false;
        life = std::max(0.8f, life - age);
    }
    // As long as it takes to read: 1.5 s and 60 ms a character, from 2.5 s to 4.5 s. Busy holds
    // until the answer replaces it, and is gone in fifteen seconds at the most.
    void say(std::string words, parts::Tone kind) {
        holdUntilPress = false;
        dot.reset();
        text = std::move(words);
        tone = kind;
        if (kind == parts::Tone::Busy) {
            life = 15.0f;
        } else {
            int chars = 0;
            for (unsigned char ch : text)
                if ((ch & 0xC0) != 0x80) ++chars;
            life = std::clamp(1.5f + 0.06f * static_cast<float>(chars), 2.5f, 4.5f);
        }
        age = 0.0f;
    }
    void clear() {
        life = 0.0f;
        holdUntilPress = false;
    }
    void tick(float dt) {
        if (life <= 0.0f) return;
        if (!holdUntilPress) life -= dt;
        age += dt;
    }
    bool showing() const { return life > 0.0f; }
    float alpha() const {
        if (life <= 0.0f) return 0.0f;
        const float in = std::min(1.0f, age / 0.25f);
        const float out = life < 0.5f ? life / 0.5f : 1.0f;   // the last half second fades
        return std::min(in, out);
    }
};
