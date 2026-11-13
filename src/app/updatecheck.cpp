// Check for updates, in 2P Start's window (app.h): when it asks, what its row says, and what is said
// when one is found. The asking is updates.cpp's.

#include "app/app.h"

namespace {

using Tone = parts::Tone;

// A minute after the start, so it does not crowd signing in, and a day after each check.
constexpr float kFirstCheckSeconds = 60.0f;
constexpr float kCheckEverySeconds = 24.0f * 60.0f * 60.0f;

}  // namespace

std::string App::updatesWord() const {
    if (!updates_) return {};
    switch (updates_->state()) {
        case updates::State::Checking: return "Checking\xE2\x80\xA6";
        case updates::State::UpToDate: return "Up to date";
        case updates::State::Available: return "Update available";
        case updates::State::Failed: return "Couldn't check";
        case updates::State::None: break;
    }
    return {};
}

void App::tickUpdates(float dt) {
    if (!updates_) return;
    sinceCheck_ += dt;
    const bool never = updates_->state() == updates::State::None;
    if ((never && sinceCheck_ >= kFirstCheckSeconds) || sinceCheck_ >= kCheckEverySeconds) {
        sinceCheck_ = 0.0f;
        updates_->start();
    }
    if (!updates_->ended()) return;
    const bool asked = updateAsked_;
    updateAsked_ = false;
    if (updates_->state() != updates::State::Available) return;
    // Pressed: the question at once. Found by itself: said once for each version, as the console's
    // weekly check says it (updateflow.cpp).
    if (asked && place_ == Place::Settings && overlay_.kind == Overlay::Kind::None) {
        askDownload();
    } else if (!asked && updateSaid_ != updates_->found().version) {
        updateSaid_ = updates_->found().version;
        if (place_ != Place::Game) notice_.say("Update available", Tone::Info);
    }
}

void App::pressUpdates() {
    if (!updates_) return;
    if (updates_->state() == updates::State::Available) {
        askDownload();
        return;
    }
    updateAsked_ = true;
    sinceCheck_ = 0.0f;
    updates_->start();
}

// "Update available": Download opens the release's page in the browser; Later leaves it.
void App::askDownload() {
    const updates::Release r = updates_->found();
    parts::Panel p;
    p.title = "Update available";
    p.detail = "Version " + r.version;
    p.options = {"Download", "Later"};
    openPanel(p, [r](int answer) {
        if (answer == 0) SDL_OpenURL(r.page.c_str());
    });
}
