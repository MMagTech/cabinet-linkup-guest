// The three kinds of hands, turned into moves (input.h).

#include "app/input.h"

#include <algorithm>
#include <cstdlib>

namespace input {

namespace {

using moves::Move;

float repeatGap(float heldFor) {
    const float t = std::clamp(heldFor / kRepeatRamp, 0.0f, 1.0f);
    return kRepeatStart + (kRepeatFast - kRepeatStart) * t;
}

bool isDirection(Move m) { return m == Move::Up || m == Move::Down || m == Move::Left || m == Move::Right; }

std::optional<Move> keyMove(SDL_Keycode key, SDL_Keymod mod) {
    switch (key) {
        case SDLK_UP: return Move::Up;
        case SDLK_DOWN: return Move::Down;
        case SDLK_LEFT: return Move::Left;
        case SDLK_RIGHT: return Move::Right;
        case SDLK_RETURN:
        case SDLK_KP_ENTER: return Move::Press;
        case SDLK_ESCAPE: return Move::Back;
        // Tab goes down a form, and with Shift back up it, as everywhere else on a computer.
        case SDLK_TAB: return (mod & SDL_KMOD_SHIFT) ? Move::Up : Move::Down;
        default: return std::nullopt;
    }
}

std::optional<Move> buttonMove(int button) {
    switch (button) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP: return Move::Up;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return Move::Down;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return Move::Left;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return Move::Right;
        case SDL_GAMEPAD_BUTTON_SOUTH: return Move::Press;
        case SDL_GAMEPAD_BUTTON_EAST: return Move::Back;
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return Move::PrevScreen;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return Move::NextScreen;
        default: return std::nullopt;
    }
}

}  // namespace

std::optional<Move> Translator::take(const SDL_Event& e, std::optional<parts::Hands>& hands) {
    switch (e.type) {
        case SDL_EVENT_KEY_DOWN: {
            hands = parts::Hands::Keyboard;
            // With Command or Control, a key is a shortcut (paste), not a move.
            if (e.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) return std::nullopt;
            const std::optional<Move> m = keyMove(e.key.key, e.key.mod);
            if (!m || e.key.repeat) return std::nullopt;
            if (isDirection(*m)) return hold(Source::Key, 0, static_cast<int>(e.key.key), *m);
            return m;
        }
        case SDL_EVENT_KEY_UP:
            release(Source::Key, 0, static_cast<int>(e.key.key));
            return std::nullopt;
        case SDL_EVENT_TEXT_INPUT:
            hands = parts::Hands::Keyboard;
            return std::nullopt;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
            hands = parts::Hands::Controller;
            const std::optional<Move> m = buttonMove(e.gbutton.button);
            if (!m) return std::nullopt;
            if (isDirection(*m)) return hold(Source::Button, e.gbutton.which, e.gbutton.button, *m);
            return m;
        }
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            release(Source::Button, e.gbutton.which, e.gbutton.button);
            return std::nullopt;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (e.gaxis.axis != SDL_GAMEPAD_AXIS_LEFTX && e.gaxis.axis != SDL_GAMEPAD_AXIS_LEFTY) return std::nullopt;
            if (std::abs(static_cast<int>(e.gaxis.value)) > kStickTouched) hands = parts::Hands::Controller;
            return stick(e.gaxis.which, e.gaxis.axis, e.gaxis.value);
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_WHEEL:
            hands = parts::Hands::Mouse;
            return std::nullopt;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            hands = parts::Hands::Mouse;
            if (e.button.button == SDL_BUTTON_RIGHT) return Move::Back;
            return std::nullopt;
        default:
            return std::nullopt;
    }
}

std::optional<Move> Translator::tick(float dt) {
    if (!held_.on) return std::nullopt;
    held_.heldFor += dt;
    if (held_.heldFor < held_.next) return std::nullopt;
    held_.next = held_.heldFor + repeatGap(held_.heldFor);
    return held_.move;
}

void Translator::letGo() {
    held_.on = false;
    stickSide_[0] = stickSide_[1] = 0;
}

std::optional<Move> Translator::hold(Source source, uint32_t device, int code, Move move) {
    held_.on = true;
    held_.source = source;
    held_.device = device;
    held_.code = code;
    held_.move = move;
    held_.heldFor = 0.0f;
    held_.next = kRepeatDelay;
    return move;
}

void Translator::release(Source source, uint32_t device, int code) {
    if (held_.on && held_.source == source && held_.device == device && held_.code == code) held_.on = false;
}

// The left stick as a d-pad: each axis has a side, which changes only past kStickOn and back under
// kStickOff. Down is positive, as SDL has it.
std::optional<Move> Translator::stick(uint32_t device, int axis, int value) {
    const int index = axis == SDL_GAMEPAD_AXIS_LEFTX ? 0 : 1;
    if (device != stickPad_) {
        stickPad_ = device;
        stickSide_[0] = stickSide_[1] = 0;
    }
    int& side = stickSide_[index];
    int now = side;
    if (value >= kStickOn) now = 1;
    else if (value <= -kStickOn) now = -1;
    else if (std::abs(value) < kStickOff) now = 0;
    if (now == side) return std::nullopt;
    side = now;
    if (now == 0) {
        release(Source::Stick, device, axis);
        return std::nullopt;
    }
    const Move m = index == 0 ? (now > 0 ? Move::Right : Move::Left) : (now > 0 ? Move::Down : Move::Up);
    return hold(Source::Stick, device, axis, m);
}

}  // namespace input
