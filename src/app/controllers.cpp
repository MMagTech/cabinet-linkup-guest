// The controllers, in 2P Start's window (app.h): every one opened, the one that plays, and one pressed
// with another program in front. And the close button in a game.

#include "app/app.h"

#include <algorithm>

#include "app/shared.h"

using namespace shared;

// Every controller is opened, so any of them works the menus. Settings says which one plays.
void App::padsChanged() {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    padIds_.assign(ids, ids + std::max(0, count));
    SDL_free(ids);
    for (SDL_JoystickID id : padIds_)
        if (!pads_.count(id))
            if (SDL_Gamepad* pad = SDL_OpenGamepad(id)) pads_[id] = pad;
    for (auto it = pads_.begin(); it != pads_.end();) {
        if (std::find(padIds_.begin(), padIds_.end(), it->first) == padIds_.end()) {
            if (it->second == sensing_) sensing_ = nullptr;
            SDL_CloseGamepad(it->second);
            it = pads_.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<std::string> App::padNames() const {
    std::vector<std::string> names;
    for (SDL_JoystickID id : padIds_) {
        const char* name = SDL_GetGamepadNameForID(id);
        names.push_back(name && *name ? name : "Controller");
    }
    return names;
}

int App::chosenPad() const {
    for (size_t i = 0; i < padIds_.size(); ++i) {
        char guid[64] = {};
        SDL_GUIDToString(SDL_GetGamepadGUIDForID(padIds_[i]), guid, sizeof guid);
        if (settings_.controller == guid) return static_cast<int>(i);
    }
    return 0;
}

// A controller pressed with another program in front, which SDL hands the app too: counted for the
// idle rule, and only that, since the menus here are not what it was pressed for.
bool App::elsewhere(const SDL_Event& e) {
    const bool pad = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type == SDL_EVENT_GAMEPAD_BUTTON_UP ||
                     e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION;
    if (!pad || (SDL_GetWindowFlags(window_) & SDL_WINDOW_INPUT_FOCUS)) return false;
    if (byHands(e)) idle_.pressed();
    return true;
}

bool App::closeInGame(const SDL_Event& e) {
    if (e.type != SDL_EVENT_WINDOW_CLOSE_REQUESTED || SDL_GetWindowFromID(e.window.windowID) != window_) return false;
    if (place_ != Place::Game || !session_) return false;
    if (overlay_.kind == Overlay::Kind::None) askLeave();
    return true;
}
