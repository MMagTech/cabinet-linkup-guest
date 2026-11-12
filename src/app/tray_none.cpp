// The window's comings and goings elsewhere (tray.h): a system with neither a Mac's menu bar nor
// Windows' tray. The close button quits, as it always did, and nothing else is said.

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

void showing(Tray::System&) {}
bool notify(Tray::System&, const Invite&) { return false; }
void withdraw(Tray::System&, const std::string&) {}
void look(Tray::System&, Status, const std::string&) {}
Reply takeReply(Tray::System&) { return {}; }

}  // namespace tray
