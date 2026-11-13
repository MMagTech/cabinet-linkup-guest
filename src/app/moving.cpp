// Moving around the screens, in 2P Start's window (app.h): the bar, the list of friends, Settings,
// first run and the panels over them, as the console's barKey, LinkUpScreen::key, the Settings list
// and choice.cpp have it.

#include "app/app.h"

#include <algorithm>

#include "app/shared.h"
#include "sound.h"
#include "ui/look.h"

namespace {

using moves::Move;
using Then = moves::Step::Then;
using Tone = parts::Tone;

constexpr int kBarCount = 2;            // LinkUp, Settings

}  // namespace

using namespace shared;

void App::move(Move m) {
    // In a line, Left and Right are its caret, a letter at a time, as in the console's keyboard
    // field: from the keys and the d-pad alike, and held, at the pace every held direction has. With
    // Shift, they choose the letters they pass.
    if ((m == Move::Left || m == Move::Right) && typing()) {
        const int by = m == Move::Right ? 1 : -1;
        const bool choosing = shiftHeld();
        edit([by, choosing](textline::Line l, textline::Kind kind) {
            return textline::step(std::move(l), by, kind, choosing);
        });
        return;
    }
    if (overlay_.kind != Overlay::Kind::None) {
        moveOnOverlay(m);
        return;
    }
    switch (place_) {
        case Place::FirstRun: moveOnFirstRun(m); return;
        case Place::Game: moveInGame(m); return;
        case Place::Friends:
        case Place::Settings: break;
    }
    if (typingName_) {
        moveOnSettings(m);
        return;
    }
    // The shoulders mean the same on every screen that has the bar.
    if (m == Move::PrevScreen || m == Move::NextScreen) {
        const moves::BarStep s = moves::bar(place_ == Place::Settings ? 1 : 0, kBarCount, m, true);
        cue(s.edge ? Then::Edge : Then::Moved);
        if (!s.edge) switchTo(s.at == 1 ? Place::Settings : Place::Friends);
        return;
    }
    if (barFocused_) moveOnBar(m);
    else if (place_ == Place::Friends) moveOnFriends(m);
    else moveOnSettings(m);
}

void App::moveOnBar(Move m) {
    const moves::BarStep s = moves::bar(place_ == Place::Settings ? 1 : 0, kBarCount, m, screenTakesFocus());
    cue(s.edge ? Then::Edge : Then::Moved);
    if (s.leave) {
        barFocused_ = false;
        return;
    }
    // Moving across the bar switches the screen under it.
    if (!s.edge) {
        switchTo(s.at == 1 ? Place::Settings : Place::Friends);
        barFocused_ = true;
    }
}

void App::switchTo(Place place) {
    if (place == place_) return;
    place_ = place;
    if (place == Place::Friends) {
        friendFocus_ = tileCount() > 0 ? std::max(friendFocus_, -1) : -1;
        barFocused_ = barFocused_ || tileCount() == 0;
    } else {
        // In on the category list, as the console comes down from its bar.
        settingsCategory_ = std::clamp(settingsCategory_, 0, static_cast<int>(screens::settingsCategories().size()) - 1);
        settingsRow_ = -1;
    }
}

int App::tileCount() const { return state_.instead.empty() ? static_cast<int>(order_.size()) : 0; }

bool App::screenTakesFocus() const { return place_ == Place::Settings || tileCount() > 0; }

void App::moveOnFriends(Move m) {
    const moves::Step s = moves::friends(friendFocus_, tileCount(), m);
    cue(s.then);
    switch (s.then) {
        case Then::Moved: friendFocus_ = s.focus; break;
        case Then::ToBar:
        case Then::Back: barFocused_ = true; break;
        case Then::Press: pressFriend(); break;
        case Then::Edge: break;
    }
}

void App::pressFriend() {
    if (tileCount() == 0) return;
    if (friendFocus_ < 0) {
        flipStatus();
        return;
    }
    const linkup::Friend& f = state_.friends[static_cast<size_t>(order_[static_cast<size_t>(friendFocus_)])];
    // Only an invite can be pressed here: this app joins, and never invites.
    if (f.status == linkup::Status::InvitedYou) answerInvite(f);
}

void App::moveOnSettings(Move m) {
    using namespace screens;
    if (typingName_) {
        if (m == Move::Press) {
            sound::play(sound::Cue::Activate);
            const std::string name = trimmed(typedName_);
            typingName_ = false;
            if (textline::ready(name, textline::Kind::Name) && name != name_) {
                name_ = name;
                link_->rename(name);
            }
        } else if (m == Move::Back) {
            sound::play(sound::Cue::Back);
            typingName_ = false;
        }
        return;
    }
    const setting::Row row = settingAtFocus();
    // A choice row takes Left and Right, and A steps it on, as the console's do: heard as a move,
    // after the change, so Interface sounds is heard at the level it just chose.
    const bool choice = row == setting::Colour || row == setting::Controller || row == setting::Sounds;
    if (choice && (m == Move::Left || m == Move::Right || m == Move::Press)) {
        cue(stepSetting(row, m == Move::Left ? -1 : 1) ? Then::Moved : Then::Edge);
        return;
    }
    std::vector<std::vector<bool>> focusable;
    const screens::SettingsView view = settingsView();
    for (const SettingsCategory& cat : settingsCategories()) {
        focusable.emplace_back();
        for (const setting::Row r : cat.rows) focusable.back().push_back(settingFocusable(r, view));
    }
    const moves::SettingsStep s = moves::settings(settingsCategory_, settingsRow_, focusable, m);
    cue(m == Move::Back && s.then == Then::Moved ? Then::Back : s.then);
    switch (s.then) {
        case Then::Moved:
            settingsCategory_ = s.category;
            settingsRow_ = s.row;
            break;
        case Then::ToBar:
        case Then::Back: barFocused_ = true; break;
        case Then::Press: pressSetting(row); break;
        case Then::Edge: break;
    }
}

// The row with the focus, or Count on the category list.
screens::setting::Row App::settingAtFocus() const {
    const auto& cats = screens::settingsCategories();
    if (settingsRow_ < 0 || settingsCategory_ < 0 || settingsCategory_ >= static_cast<int>(cats.size()))
        return screens::setting::Count;
    const auto& rows = cats[static_cast<size_t>(settingsCategory_)].rows;
    return settingsRow_ < static_cast<int>(rows.size()) ? rows[static_cast<size_t>(settingsRow_)] : screens::setting::Count;
}

void App::pressSetting(int row) {
    using namespace screens;
    switch (row) {
        case setting::Name:
            typingName_ = true;
            typedName_ = name_;
            caretToEnd();
            still_ = 0.0f;
            break;
        case setting::Colour:
        case setting::Controller:
        case setting::Sounds: stepSetting(row, 1); break;
        case setting::FullScreen: setFullScreen(!settings_.fullScreen); break;
        case setting::KeepRunning: setKeepRunning(!settings_.keepRunning); break;
        case setting::Startup: setStartup(!settings_.startup); break;
        case setting::Updates: pressUpdates(); break;
        case setting::Server: serverPanel(); break;
        case setting::HowTo:
        case setting::Credits:
            overlay_ = Overlay{};
            overlay_.kind = row == setting::HowTo ? Overlay::Kind::HowTo : Overlay::Kind::Credits;
            break;
        default: break;
    }
}

// A choice row: one along, stopping at either end, as the console's do. False at an end.
bool App::stepSetting(int row, int by) {
    if (row == screens::setting::Colour) {
        const int now = colourIndex(settings_.colour);
        const int next = std::clamp(now + by, 0, look::kColourCount - 1);
        if (next == now) return false;
        settings_.colour = look::colourWord(static_cast<look::Colour>(next));
    } else if (row == screens::setting::Controller) {
        const int count = static_cast<int>(padIds_.size());
        const int now = chosenPad();
        const int next = std::clamp(now + by, 0, std::max(0, count - 1));
        if (count == 0 || next == now) return false;
        char guid[64] = {};
        SDL_GUIDToString(SDL_GetGamepadGUIDForID(padIds_[static_cast<size_t>(next)]), guid, sizeof guid);
        settings_.controller = guid;
    } else if (row == screens::setting::Sounds) {
        const int now = static_cast<int>(soundLevel(settings_.sounds));
        const int next = std::clamp(now + by, 0, sound::kLevelCount - 1);
        if (next == now) return false;
        sound::setLevel(static_cast<sound::Level>(next));
        settings_.sounds = sound::levelWord(static_cast<sound::Level>(next));
    } else {
        return false;
    }
    keepSettings();
    return true;
}

void App::moveOnFirstRun(Move m) {
    if (first_.connecting) return;
    const moves::Step s = moves::column(first_.focus, 4, m);
    if (s.then != Then::Back) cue(s.then);
    switch (s.then) {
        case Then::Moved:
            first_.focus = s.focus;
            caretToEnd();
            break;
        // Enter on a line goes to the next, as Tab does; on Connect, it connects.
        case Then::Press:
            if (first_.focus < 3) {
                first_.focus += 1;
                caretToEnd();
            } else {
                connect();
            }
            break;
        default: break;
    }
}

void App::moveOnOverlay(Move m) {
    Overlay& o = overlay_;
    if (o.kind == Overlay::Kind::Field) {
        if (m == Move::Press) {
            const std::string value = o.field.value;
            if (o.fieldKind == textline::Kind::Code && !textline::ready(value, o.fieldKind)) {
                notice_.say(kCodeLength, Tone::Problem);
                return;
            }
            if (!textline::ready(value, o.fieldKind)) {
                cue(Then::Edge);
                return;
            }
            cue(Then::Press);
            auto typed = o.typed;
            closeOverlay();
            if (typed) typed(value);
        } else if (m == Move::Back) {
            cue(Then::Back);
            closeOverlay();
        }
        return;
    }
    if (o.reading()) {
        // Nothing to choose: A or B closes it.
        if (m == Move::Press || m == Move::Back) {
            cue(Then::Back);
            closeOverlay();
            return;
        }
        const int lines = o.kind == Overlay::Kind::HowTo ? screens::howToCount() : screens::creditCount();
        const moves::Step s = moves::column(o.panel.focused, lines, m);
        cue(s.then);
        if (s.then == Then::Moved) {
            o.panel.focused = s.focus;
            o.panel.top = moves::topFor(s.focus, o.panel.top, lines, parts::kPanelRows);
        }
        return;
    }
    // A line with only a prompt: A goes on, and nothing else does.
    if (o.prompt) {
        if (m == Move::Press) {
            cue(Then::Press);
            auto then = o.then;
            closeOverlay();
            if (then) then(0);
        }
        return;
    }
    const moves::Step s = moves::column(o.panel.focused, static_cast<int>(o.panel.options.size()), m);
    cue(s.then);
    if (s.then == Then::Moved) {
        o.panel.focused = s.focus;
        o.panel.top = moves::topFor(s.focus, o.panel.top, static_cast<int>(o.panel.options.size()), parts::kPanelRows);
    } else if (s.then == Then::Press || s.then == Then::Back) {
        const int answer = s.then == Then::Press ? s.focus : -1;
        auto then = o.then;
        closeOverlay();
        if (then) then(answer);
    }
}
