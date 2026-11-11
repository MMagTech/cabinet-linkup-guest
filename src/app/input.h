// A controller, the keys and the mouse, all at once, turned into the console's moves (moves.h), and
// which of them was touched last, so the hints can name it.
//
// A HELD DIRECTION REPEATS AT THE CONSOLE'S PACE (design.h, HeldRepeat): the move at once, again
// after 0.42 s, then faster, from 0.12 s down to 0.045 s over 1.2 s held. The keyboard's own repeat
// is not used for moving, so a held arrow and a held d-pad feel the same. A click is the app's to
// place, since only it knows what is under the pointer; a right-click is Back.

#pragma once

#include <cstdint>
#include <optional>

#include <SDL3/SDL.h>

#include "app/moves.h"
#include "ui/parts.h"

namespace input {

constexpr float kRepeatDelay = 0.420f;
constexpr float kRepeatStart = 0.120f;
constexpr float kRepeatFast = 0.045f;
constexpr float kRepeatRamp = 1.200f;

// A stick counts as pushed past half way, and as let go under a third, so it does not flicker on
// the line. Less than kStickTouched is a stick resting off centre, and touches nothing.
constexpr int kStickOn = 16384;
constexpr int kStickOff = 11000;
constexpr int kStickTouched = 8000;

class Translator {
public:
    // The move `e` asks for, if any. `hands` is set when `e` came from somebody's hands, also for one
    // that asks for no move: the mouse moving, or a key typed into a line.
    std::optional<moves::Move> take(const SDL_Event& e, std::optional<parts::Hands>& hands);
    // A held direction's next repeat, if one is due after `dt` seconds more.
    std::optional<moves::Move> tick(float dt);
    // Something is held, so the loop has to keep counting time.
    bool holding() const { return held_.on; }
    // Everything let go: the window lost focus, and a key's release will not arrive.
    void letGo();

private:
    enum class Source { Key, Button, Stick };
    struct Held {
        bool on = false;
        Source source = Source::Key;
        uint32_t device = 0;
        int code = 0;
        moves::Move move = moves::Move::Up;
        float heldFor = 0.0f, next = 0.0f;
    };
    std::optional<moves::Move> hold(Source source, uint32_t device, int code, moves::Move move);
    void release(Source source, uint32_t device, int code);
    std::optional<moves::Move> stick(uint32_t device, int axis, int value);

    Held held_;
    // Each stick axis's side, -1, 0 or 1, for the last pad that moved it.
    uint32_t stickPad_ = 0;
    int stickSide_[2] = {0, 0};
};

}  // namespace input
