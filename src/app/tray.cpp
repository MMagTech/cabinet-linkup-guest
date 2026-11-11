// The window's comings and goings (tray.h): the part every system shares.

#include "app/tray.h"

#include <cstdio>

#include "app/tray_system.h"

namespace tray {

void push(uint32_t event, Ask ask) {
    SDL_Event e{};
    e.type = event;
    e.user.code = static_cast<int32_t>(ask);
    SDL_PushEvent(&e);
}

// FNV-1a, 64 bits: the same name for the same folder, in every copy and every version.
std::string folderKey(const std::string& folder) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : folder) {
        h ^= c;
        h *= 1099511628211ull;
    }
    char text[17];
    std::snprintf(text, sizeof text, "%016llx", static_cast<unsigned long long>(h));
    return text;
}

bool Tray::take(const SDL_Event& e) {
    if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        if (SDL_GetWindowFromID(e.window.windowID) != window_) return false;
        hide(*system_, window_, asks_);
        return true;
    }
    if (e.type != eventOf(*system_)) return false;
    switch (static_cast<Ask>(e.user.code)) {
        case Ask::Open: open(); break;
        case Ask::Status:
            if (asks_.flipStatus) asks_.flipStatus();
            break;
        case Ask::Answer: {
            const std::string who = takeAnswer(*system_);
            open();
            if (!who.empty() && asks_.answer) asks_.answer(who);
            break;
        }
    }
    return true;
}

void Tray::open() {
    if (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) SDL_RestoreWindow(window_);
    SDL_ShowWindow(window_);
    SDL_RaiseWindow(window_);
}

bool Tray::hiddenAway() const {
    const SDL_WindowFlags f = SDL_GetWindowFlags(window_);
    return (f & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)) != 0 || (f & SDL_WINDOW_INPUT_FOCUS) == 0;
}

void Tray::invited(const std::string& who, const std::string& title, const std::string& game) {
    if (hiddenAway()) notify(*system_, who, title, game);
}

}  // namespace tray
