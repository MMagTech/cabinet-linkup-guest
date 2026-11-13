// What the files of 2P Start's window (app.h) share: the console's sound for a move, Settings' words
// read back, the keys held, a name trimmed, and what counts as a person's hands.

#pragma once

#include <cstdlib>
#include <string>

#include <SDL3/SDL.h>

#include "app/idle.h"
#include "app/moves.h"
#include "sound.h"
#include "ui/look.h"

namespace shared {

inline constexpr const char* kCodeLength = "A code has 12 characters";

// The console's sound for what a move did (bar.cpp, settings.cpp, linkup.cpp): focus moved, nothing
// there, something chosen, or back. A pointer resting somewhere makes none: it is not a move.
inline void cue(moves::Step::Then then) {
    using Then = moves::Step::Then;
    switch (then) {
        case Then::Moved:
        case Then::ToBar: sound::play(sound::Cue::Move); break;
        case Then::Edge: sound::play(sound::Cue::Edge); break;
        case Then::Press: sound::play(sound::Cue::Activate); break;
        case Then::Back: sound::play(sound::Cue::Back); break;
    }
}

// The level stored in settings.json, by the console's word; Medium for anything else.
inline sound::Level soundLevel(const std::string& word) {
    sound::Level level = sound::Level::Medium;
    sound::levelFromWord(word, &level);
    return level;
}

inline int colourIndex(const std::string& word) {
    for (int i = 0; i < look::kColourCount; ++i)
        if (word == look::colourWord(static_cast<look::Colour>(i))) return i;
    return 0;
}

inline bool shiftHeld() { return (SDL_GetModState() & SDL_KMOD_SHIFT) != 0; }

inline std::string trimmed(const std::string& s) {
    const size_t first = s.find_first_not_of(' ');
    if (first == std::string::npos) return {};
    return s.substr(first, s.find_last_not_of(' ') - first + 1);
}

// A person's hands, as the console's idle rule counts them: a press, the mouse moved, or a stick
// pushed past where it rests.
inline bool byHands(const SDL_Event& e) {
    switch (e.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_WHEEL:
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN: return true;
        case SDL_EVENT_MOUSE_MOTION: return e.motion.xrel != 0.0f || e.motion.yrel != 0.0f;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: return std::abs(static_cast<int>(e.gaxis.value)) > idle::kAxisDeadzone;
        default: return false;
    }
}

}  // namespace shared
