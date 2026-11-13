// For the tray (tray.h), in 2P Start's window (app.h): the invites waiting, an answer from the tray's
// menu or a notification, the Status line, and the first hide, said once.

#include "app/app.h"

void App::tellInvites(std::function<bool(const std::string&, const std::string&, const std::string&)> onInvite) {
    onInvite_ = std::move(onInvite);
}

std::vector<App::Waiting> App::invites() const {
    std::vector<Waiting> out;
    if (!paired_ || !state_.instead.empty()) return out;
    for (int index : order_) {
        const linkup::Friend& f = state_.friends[static_cast<size_t>(index)];
        if (f.status != linkup::Status::InvitedYou) continue;
        out.push_back({f.name, linkup::invitedYou(f.name), f.game});
    }
    return out;
}

// From the tray's menu or a notification: the list, the friend's tile, and their invite's question,
// as a press on it; or its answer, given on the notification. Nothing in a game, or over a question
// or a line being typed.
void App::reply(const std::string& who, int answer) {
    if (place_ != Place::Friends && place_ != Place::Settings) return;
    if (typingName_ || (overlay_.kind != Overlay::Kind::None && overlay_.answering != who)) return;
    const int count = tileCount();
    for (int i = 0; i < count; ++i) {
        const linkup::Friend& f = state_.friends[static_cast<size_t>(order_[static_cast<size_t>(i)])];
        if (f.name != who || f.status != linkup::Status::InvitedYou) continue;
        if (overlay_.kind != Overlay::Kind::None) closeOverlay();
        if (answer == 1) {
            decline(f);
            return;
        }
        switchTo(Place::Friends);
        barFocused_ = false;
        friendFocus_ = i;
        if (answer == 0) join(f);
        else answerInvite(f);
        return;
    }
}

bool App::seenOnline() const { return paired_ && state_.instead.empty() && statusWord() == "online"; }

// The Status line: Appear Offline by another name, and kept.
void App::flipStatus() {
    if (!paired_) return;
    settings_.appearOffline = !settings_.appearOffline;
    state_.appearOffline = settings_.appearOffline;
    keepSettings();
    link_->setStatus(statusWord());
}

bool App::sayStillRunning() {
    if (settings_.saidTray) return false;
    settings_.saidTray = true;
    keepSettings();
    return true;
}
