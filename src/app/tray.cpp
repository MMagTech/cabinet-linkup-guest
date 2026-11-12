// The window's comings and goings (tray.h): the part every system shares.

#include "app/tray.h"

#include <algorithm>
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

Status statusOf(const std::vector<Invite>& waiting, bool online) {
    if (!waiting.empty()) return Status::Invite;
    return online ? Status::Online : Status::Offline;
}

std::string tipOf(const std::vector<Invite>& waiting, bool online) {
    if (!waiting.empty()) return waiting.front().title;
    return online ? "2P Start: Online" : "2P Start: Offline";
}

std::string lineOf(const Invite& invite) {
    return invite.game.empty() ? invite.title : invite.title + ": " + invite.game;
}

bool Tray::take(const SDL_Event& e) {
    if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        if (SDL_GetWindowFromID(e.window.windowID) != window_) return false;
        if (!asks_.keepRunning || asks_.keepRunning()) {
            hide(*system_, window_, asks_);
        } else {
            SDL_Event quit{};
            quit.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&quit);
        }
        return true;
    }
    if (e.type != eventOf(*system_)) return false;
    switch (static_cast<Ask>(e.user.code)) {
        case Ask::Open: open(); break;
        case Ask::Online:
        case Ask::Offline:
            if (asks_.setOnline) asks_.setOnline(static_cast<Ask>(e.user.code) == Ask::Online);
            break;
        case Ask::Reply: {
            const Reply r = takeReply(*system_);
            // Decline needs no window; Join and the question do.
            if (r.answer != 1) open();
            if (!r.who.empty() && asks_.reply) asks_.reply(r.who, r.answer);
            break;
        }
        case Ask::Startup:
            if (asks_.flipStartup && (!asks_.canStartup || asks_.canStartup())) asks_.flipStartup();
            break;
    }
    return true;
}

void Tray::open() {
    showing(*system_);
    if (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) SDL_RestoreWindow(window_);
    SDL_ShowWindow(window_);
    SDL_RaiseWindow(window_);
}

bool Tray::hiddenAway() const {
    const SDL_WindowFlags f = SDL_GetWindowFlags(window_);
    return (f & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)) != 0 || (f & SDL_WINDOW_INPUT_FOCUS) == 0;
}

bool Tray::invited(const Invite& invite) {
    return hiddenAway() && notify(*system_, invite);
}

void Tray::follow(const std::vector<Invite>& waiting, bool online) {
    const Status status = statusOf(waiting, online);
    const std::string tip = tipOf(waiting, online);
    if (status != shown_ || tip != tip_) {
        shown_ = status;
        tip_ = tip;
        look(*system_, status, tip);
    }
    std::vector<std::string> now;
    for (const Invite& i : waiting) now.push_back(i.who);
    for (const std::string& who : waiting_) {
        if (std::find(now.begin(), now.end(), who) == now.end()) withdraw(*system_, who);
    }
    waiting_ = std::move(now);
}

}  // namespace tray
