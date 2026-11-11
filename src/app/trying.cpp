// Steps played into the app (trying.h).

#include "app/trying.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "app/app.h"
#include "ui/draw.h"

namespace {

// Where a point on the canvas is in the window, as Canvas::setOutput fits one into the other.
void toWindow(SDL_Renderer* renderer, float x, float y, float& wx, float& wy) {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(renderer, &w, &h);
    const float scale = std::min(static_cast<float>(w) / ui::kCanvasWidth, static_cast<float>(h) / ui::kCanvasHeight);
    const float px = (static_cast<float>(w) - ui::kCanvasWidth * scale) * 0.5f + x * scale;
    const float py = (static_cast<float>(h) - ui::kCanvasHeight * scale) * 0.5f + y * scale;
    SDL_RenderCoordinatesToWindow(renderer, px, py, &wx, &wy);
}

int buttonNamed(const std::string& name) {
    if (name == "a") return SDL_GAMEPAD_BUTTON_SOUTH;
    if (name == "b") return SDL_GAMEPAD_BUTTON_EAST;
    if (name == "up") return SDL_GAMEPAD_BUTTON_DPAD_UP;
    if (name == "down") return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    if (name == "left") return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
    if (name == "right") return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    if (name == "lb") return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    if (name == "rb") return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    return -1;
}

}  // namespace

bool Trying::load(const std::string& path, std::string& why) {
    std::ifstream in(path);
    if (!in) {
        why = "cannot read " + path;
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        steps_.push_back(line);
    }
    return true;
}

bool Trying::play(float dt, App& app, SDL_Renderer* renderer) {
    waiting_ -= dt;
    while (waiting_ <= 0.0f && shot_.empty()) {
        if (steps_.empty()) return false;
        const std::string line = steps_.front();
        steps_.pop_front();
        std::istringstream words(line);
        std::string what;
        words >> what;
        std::string rest;
        std::getline(words, rest);
        if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
        std::printf("try: %s\n", line.c_str());
        std::fflush(stdout);
        SDL_Event e{};
        if (what == "wait") {
            waiting_ = std::stof(rest);
        } else if (what == "key") {
            // A key held with it, after the key's name: `key Left cmd`.
            const size_t space = rest.rfind(' ');
            const std::string held = space == std::string::npos ? "" : rest.substr(space + 1);
            const SDL_Keymod mod = held == "cmd"     ? SDL_KMOD_LGUI
                                   : held == "ctrl"  ? SDL_KMOD_LCTRL
                                   : held == "shift" ? SDL_KMOD_LSHIFT
                                                     : SDL_KMOD_NONE;
            const std::string name = mod == SDL_KMOD_NONE ? rest : rest.substr(0, space);
            e.type = SDL_EVENT_KEY_DOWN;
            e.key.key = SDL_GetKeyFromName(name.c_str());
            e.key.mod = mod;
            e.key.down = true;
            app.handle(e);
            e.type = SDL_EVENT_KEY_UP;
            e.key.down = false;
            app.handle(e);
        } else if (what == "type") {
            e.type = SDL_EVENT_TEXT_INPUT;
            e.text.text = rest.c_str();
            app.handle(e);
        } else if (what == "pad") {
            const int button = buttonNamed(rest);
            if (button < 0) continue;
            e.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
            e.gbutton.which = 1;
            e.gbutton.button = static_cast<Uint8>(button);
            e.gbutton.down = true;
            app.handle(e);
            e.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
            e.gbutton.down = false;
            app.handle(e);
        } else if (what == "point" || what == "click" || what == "rightclick") {
            float x = 0, y = 0;
            std::istringstream at(rest);
            at >> x >> y;
            float wx = 0, wy = 0;
            toWindow(renderer, x, y, wx, wy);
            e.type = SDL_EVENT_MOUSE_MOTION;
            e.motion.x = wx;
            e.motion.y = wy;
            e.motion.xrel = 1.0f;
            app.handle(e);
            if (what != "point") {
                SDL_Event b{};
                b.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
                b.button.button = what == "click" ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT;
                b.button.down = true;
                b.button.x = wx;
                b.button.y = wy;
                app.handle(b);
                b.type = SDL_EVENT_MOUSE_BUTTON_UP;
                b.button.down = false;
                app.handle(b);
            }
        } else if (what == "shot") {
            shot_ = rest;
        } else if (what == "quit") {
            return false;
        } else {
            std::fprintf(stderr, "try: no such step: %s\n", line.c_str());
        }
    }
    return true;
}

void Trying::takeShot(SDL_Renderer* renderer) {
    if (shot_.empty()) return;
    if (SDL_Surface* s = SDL_RenderReadPixels(renderer, nullptr)) {
        if (!SDL_SavePNG(s, shot_.c_str())) std::fprintf(stderr, "try: could not save %s\n", shot_.c_str());
        SDL_DestroySurface(s);
    }
    shot_.clear();
}
