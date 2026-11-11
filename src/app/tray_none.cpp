// The window's comings and goings elsewhere (tray.h): a system with neither a Mac's Dock nor Windows'
// tray. The close button quits, as it always did, and nothing else is said.

#include "app/tray.h"

#include "app/tray_system.h"

struct tray::Tray::System {
    uint32_t event = 0;
};

namespace tray {

bool another(const std::string&, bool) { return false; }

Tray::Tray(SDL_Window* window, const std::string&, Asks asks)
    : window_(window), asks_(std::move(asks)), system_(std::make_unique<System>()) {
    system_->event = SDL_RegisterEvents(1);
}

Tray::~Tray() = default;

uint32_t eventOf(const Tray::System& s) { return s.event; }

void hide(Tray::System&, SDL_Window*, const Asks&) {
    SDL_Event e{};
    e.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&e);
}

void notify(Tray::System&, const std::string&, const std::string&, const std::string&) {}

std::string takeAnswer(Tray::System&) { return {}; }

}  // namespace tray
