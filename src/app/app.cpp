// 2P Start's window (app.h). Each part names what it follows on the console.

#include "app/app.h"

#include <algorithm>

#include "guest/account.h"
#include "ui/look.h"

namespace {

using moves::Move;
using Then = moves::Step::Then;
using Tone = parts::Tone;

constexpr int kBarCount = 2;            // LinkUp, Settings
constexpr float kPairingSeconds = 40.0f;
constexpr float kInvitedSeconds = 6.0f;
constexpr const char* kCodeLength = "A code has 12 characters";
constexpr const char* kNotYet = "Not supported yet";

int colourIndex(const std::string& word) {
    for (int i = 0; i < look::kColourCount; ++i)
        if (word == look::colourWord(static_cast<look::Colour>(i))) return i;
    return 0;
}

std::string trimmed(const std::string& s) {
    const size_t first = s.find_first_not_of(' ');
    if (first == std::string::npos) return {};
    return s.substr(first, s.find_last_not_of(' ') - first + 1);
}

// The friend's own name for the console's "this console": the app.
std::string instead(guest::Link::Connection c) {
    switch (c) {
        case guest::Link::Connection::Ok: return {};
        case guest::Link::Connection::Unreachable: return linkup::kCantConnect;
        case guest::Link::Connection::Removed: return linkup::kConsoleRemoved;
        case guest::Link::Connection::AppBehind: return "2P Start needs an update";
        case guest::Link::Connection::ServerBehind: return linkup::kServerNeedsUpdate;
    }
    return {};
}

}  // namespace

App::App(SDL_Window* window, SDL_Renderer* renderer, const std::string& fonts, std::string folder, Uint32 wake)
    : window_(window), renderer_(renderer), canvas_(renderer, fonts), folder_(std::move(folder)), wake_(wake) {}

App::~App() {
    for (auto& [id, pad] : pads_) SDL_CloseGamepad(pad);
}

bool App::start(std::string& why) {
    if (!canvas_.ready()) {
        why = "the fonts are missing";
        return false;
    }
    if (folder_.empty()) {
        why = "this computer names no folder to keep a key in";
        return false;
    }
    settings_ = guest::readSettings(folder_);
    name_ = guest::username(folder_);
    state_.on = true;
    state_.appearOffline = settings_.appearOffline;
    const Uint32 wake = wake_;
    link_ = std::make_unique<guest::Link>(folder_, [wake]() {
        SDL_Event e{};
        e.type = wake;
        SDL_PushEvent(&e);
    });
    if (!link_->start()) {
        why = "no key can be kept in " + folder_;
        return false;
    }
    if (settings_.fullScreen) SDL_SetWindowFullscreen(window_, true);
    if (link_->paired()) {
        paired_ = state_.paired = true;
        place_ = Place::Friends;
        barFocused_ = true;
        link_->signIn(statusWord());
    } else {
        toFirstRun();
    }
    padsChanged();
    return true;
}

void App::quit() {
    if (link_) link_->stop(3);
}

// ---- The server (linkupOnEvent) ------------------------------------------------

void App::fromServer() {
    for (guest::Link::Event& e : link_->take()) {
        using Kind = guest::Link::Event::Kind;
        switch (e.kind) {
            case Kind::State: tookState(e); break;
            case Kind::Paired: tookPaired(e); break;
            case Kind::Friends: tookFriends(std::move(e.friends)); break;
            case Kind::Invited:
                // A person joining gets no line about anybody else.
                if (place_ == Place::Waiting) break;
                notice_.say(linkup::invitedYou(e.name), Tone::Info);
                notice_.dot = screens::friendDot(state_.friends, e.name);
                notice_.life = kInvitedSeconds;
                notice_.holdUntilPress = true;
                break;
            case Kind::InviteEnded: tookInviteEnded(e.invite); break;
            case Kind::Joined: tookJoined(e); break;
            case Kind::SessionEnded: break;
            case Kind::Left: toFirstRun(); break;
            case Kind::Retyped: tookRetyped(e); break;
        }
    }
}

void App::tookState(const guest::Link::Event& e) {
    paired_ = state_.paired = e.paired;
    address_ = e.address;
    state_.server = e.address;
    state_.instead = instead(e.connection);
    if (!e.paired) {
        if (place_ != Place::FirstRun) toFirstRun();
        return;
    }
    tookFriends(state_.friends);
}

// linkupTookFriends, and LinkUpScreen::setState's focus: it follows the friend it was on.
void App::tookFriends(std::vector<linkup::Friend> list) {
    const int before = tileCount();
    std::string wasOn;
    if (friendFocus_ >= 0 && friendFocus_ < before)
        wasOn = state_.friends[static_cast<size_t>(order_[static_cast<size_t>(friendFocus_)])].id;
    state_.friends = std::move(list);
    order_ = screens::tileOrder(state_.friends);
    const int now = tileCount();
    if (!wasOn.empty()) {
        friendFocus_ = -1;
        for (int i = 0; i < now; ++i)
            if (state_.friends[static_cast<size_t>(order_[static_cast<size_t>(i)])].id == wasOn) friendFocus_ = i;
    }
    // A first friend is a different screen: focus starts on them.
    if (before == 0 && now > 0) {
        friendFocus_ = 0;
        if (place_ == Place::Friends) barFocused_ = false;
    }
    friendFocus_ = std::min(friendFocus_, now - 1);
    // Nothing to focus below: focus goes up where it can be seen.
    if (place_ == Place::Friends && now == 0) barFocused_ = true;
}

// linkupSaidPairing.
void App::tookPaired(const guest::Link::Event& e) {
    const bool fromSettings = changing_;
    changing_ = false;
    first_.connecting = false;
    pairingLeft_ = 0.0f;
    if (e.pairResult != pairing::Result::Paired) {
        sayPairing(e.pairResult);
        return;
    }
    notice_.clear();
    paired_ = state_.paired = true;
    if (fromSettings) return;   // Link signs in again by itself, under the new server
    name_ = trimmed(first_.name);
    state_.friends.clear();
    order_.clear();
    place_ = Place::Friends;
    barFocused_ = true;
    friendFocus_ = -1;
    link_->signIn(statusWord());
}

void App::sayPairing(pairing::Result r) {
    // The console's own lines (linkupSaidPairing, docs/SETTINGS.md), "this console" left out.
    const char* words = r == pairing::Result::WrongCode      ? "Wrong or expired code"
                        : r == pairing::Result::TooManyTries  ? "Too many tries"
                        : r == pairing::Result::NotThatServer ? "That server failed its check"
                        : r == pairing::Result::Refused       ? "That server refused"
                                                              : "Can't reach that server";
    notice_.say(words, Tone::Problem);
}

// linkupInviteEnded: the line goes, and on the Join or Decline panel the person is told, until A,
// so a press meant for Join does not land on the list under it.
void App::tookInviteEnded(const std::string& invite) {
    std::string who;
    for (linkup::Friend& f : state_.friends) {
        if (f.invite != invite) continue;
        who = f.name;
        f.invite.clear();
        f.game.clear();
        if (f.status == linkup::Status::InvitedYou) f.status = linkup::Status::Online;
    }
    if (who.empty()) return;
    tookFriends(state_.friends);
    if (notice_.text == linkup::invitedYou(who)) notice_.clear();
    if (overlay_.kind == Overlay::Kind::Panel && overlay_.answering == who) openPrompt(linkup::inviteEnded(who), "", "to continue");
}

// What a join comes to, until the game can be shown: the session the server opened is ended at
// once, so the host is never left waiting on a friend who cannot play. Their console says the
// friend left.
void App::tookJoined(const guest::Link::Event& e) {
    if (e.ok) link_->end(e.session);
    const bool left = joinLeft_;
    joinLeft_ = false;
    if (left || place_ != Place::Waiting) return;
    place_ = Place::Friends;
    barFocused_ = tileCount() == 0;
    if (e.ok) openPrompt(kNotYet, "", "to continue");
    else openPrompt(linkup::notAvailable(joinHost_), "", "to continue");
}

// cmdRetype's answer: another server is asked for its code; the same, or nothing yet, is kept.
void App::tookRetyped(const guest::Link::Event& e) {
    if (e.answers == ServerLink::Place::Another) askCode(e.address);
}

std::string App::statusWord() const { return settings_.appearOffline ? "offline" : "online"; }

// ---- A frame ---------------------------------------------------------------

void App::tick(float dt) {
    clock_ += dt;
    still_ += dt;
    notice_.tick(dt);
    if (std::optional<Move> m = input_.tick(dt)) move(*m);
    fromServer();
    if (pairingLeft_ > 0.0f) {
        pairingLeft_ -= dt;
        // Nothing came back at all (tickLinkUp).
        if (pairingLeft_ <= 0.0f && first_.connecting) {
            first_.connecting = false;
            notice_.say("Can't reach that server", Tone::Problem);
        }
    }
    const bool want = typing();
    if (want != textOn_) {
        textOn_ = want;
        if (want) SDL_StartTextInput(window_);
        else SDL_StopTextInput(window_);
    }
}

bool App::busy() const {
    return place_ == Place::FirstRun || place_ == Place::Waiting || typingName_ ||
           overlay_.kind == Overlay::Kind::Field || notice_.showing() || input_.holding();
}

void App::draw() {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(renderer_, &w, &h);
    canvas_.setOutput(w, h);
    canvas_.clearSpots();
    canvas_.setLook(look::of(static_cast<look::Colour>(colourIndex(settings_.colour))));
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    switch (place_) {
        case Place::FirstRun: {
            screens::FirstRunView v = first_;
            v.caret = caret_;
            v.clock = clock_;
            v.still = still_;
            screens::drawFirstRun(canvas_, v);
            break;
        }
        case Place::Friends: {
            screens::FriendsView v;
            v.state = state_;
            v.focus = friendFocus_;
            v.barFocused = barFocused_;
            screens::drawFriends(canvas_, v);
            break;
        }
        case Place::Settings: {
            screens::SettingsView v;
            v.name = typingName_ ? typedName_ : name_;
            v.typingName = typingName_;
            v.caret = caret_;
            v.still = still_;
            v.colour = colourIndex(settings_.colour);
            v.fullScreen = settings_.fullScreen;
            v.controllers = padNames();
            v.controller = chosenPad();
            v.server = address_;
            v.focus = settingsFocus_;
            v.barFocused = barFocused_;
            screens::drawSettings(canvas_, v);
            break;
        }
        case Place::Waiting: {
            screens::WaitingView v;
            v.title = linkup::waitingFor(joinHost_);
            v.game = joinGame_;
            v.hands = hands_;
            v.clock = clock_;
            screens::drawWaiting(canvas_, v);
            break;
        }
    }
    switch (overlay_.kind) {
        case Overlay::Kind::None: break;
        case Overlay::Kind::Panel: {
            parts::Panel p = overlay_.panel;
            if (overlay_.prompt) p.prompt = parts::prompt(hands_, false, overlay_.after);
            parts::panel(canvas_, p);
            break;
        }
        case Overlay::Kind::Credits: {
            screens::CreditsView v;
            v.focus = overlay_.panel.focused;
            v.top = overlay_.panel.top;
            screens::drawCredits(canvas_, v);
            break;
        }
        case Overlay::Kind::Field: {
            parts::FieldPanel f = overlay_.field;
            f.caret = caret_;
            f.still = still_;
            f.prompt = parts::prompt(hands_, false, "to continue");
            parts::fieldPanel(canvas_, f);
            break;
        }
    }
    if (notice_.showing()) {
        canvas_.setAlpha(notice_.alpha());
        parts::notice(canvas_, notice_.text, notice_.tone, notice_.dot ? &*notice_.dot : nullptr);
        canvas_.setAlpha(1.0f);
    }
}

// ---- Hands -----------------------------------------------------------------

void App::handle(const SDL_Event& e) {
    switch (e.type) {
        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
            padsChanged();
            return;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            input_.letGo();
            return;
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
        case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN: {
            const bool on = e.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN;
            if (settings_.fullScreen != on) {
                settings_.fullScreen = on;
                keepSettings();
            }
            return;
        }
        case SDL_EVENT_TEXT_INPUT:
            if (typing() && e.text.text) typeText(e.text.text);
            break;
        case SDL_EVENT_KEY_DOWN:
            notice_.pressed();
            setHands(parts::Hands::Keyboard);
            if (e.key.key == SDLK_F11 && !e.key.repeat) {
                setFullScreen(!settings_.fullScreen);
                return;
            }
            if (typing()) {
                if (typeKey(e.key)) return;
            }
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            notice_.pressed();
            break;
        case SDL_EVENT_MOUSE_MOTION: {
            // A move of nothing is the system's, not the person's: a window appearing under the
            // pointer, or the pointer being shown.
            if (e.motion.xrel == 0.0f && e.motion.yrel == 0.0f) return;
            float x = 0, y = 0;
            SDL_RenderCoordinatesFromWindow(renderer_, e.motion.x, e.motion.y, &x, &y);
            setHands(parts::Hands::Mouse);
            pointAt(x, y, false);
            return;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            notice_.pressed();
            if (e.button.button == SDL_BUTTON_LEFT) {
                float x = 0, y = 0;
                SDL_RenderCoordinatesFromWindow(renderer_, e.button.x, e.button.y, &x, &y);
                setHands(parts::Hands::Mouse);
                pointAt(x, y, true);
                return;
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            if (overlay_.kind == Overlay::Kind::Credits && e.wheel.y != 0.0f) {
                setHands(parts::Hands::Mouse);
                move(e.wheel.y > 0.0f ? Move::Up : Move::Down);
                return;
            }
            break;
        default:
            break;
    }
    std::optional<parts::Hands> hands;
    const std::optional<Move> m = input_.take(e, hands);
    if (hands) setHands(*hands);
    if (m) move(*m);
}

void App::setHands(parts::Hands hands) {
    if (hands == hands_) return;
    hands_ = hands;
    // The pointer stays out of the way while it is not what is being used.
    if (hands == parts::Hands::Mouse) SDL_ShowCursor();
    else SDL_HideCursor();
}

// ---- Moving (barKey, LinkUpScreen::key, the Settings list, choice.cpp) ------------------------

void App::move(Move m) {
    // In a line, Left and Right are its caret, a letter at a time, as in the console's keyboard
    // field: from the keys and the d-pad alike, and held, at the pace every held direction has.
    if ((m == Move::Left || m == Move::Right) && typing()) {
        const int by = m == Move::Right ? 1 : -1;
        edit([by](textline::Line l, textline::Kind kind) { return textline::step(std::move(l), by, kind); });
        return;
    }
    if (overlay_.kind != Overlay::Kind::None) {
        moveOnOverlay(m);
        return;
    }
    switch (place_) {
        case Place::FirstRun: moveOnFirstRun(m); return;
        case Place::Waiting:
            // Leaving before the host's answer: the join is ended as soon as it is answered.
            if (m == Move::Back) {
                joinLeft_ = true;
                place_ = Place::Friends;
                barFocused_ = tileCount() == 0;
            }
            return;
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
        if (!s.edge) switchTo(s.at == 1 ? Place::Settings : Place::Friends);
        return;
    }
    if (barFocused_) moveOnBar(m);
    else if (place_ == Place::Friends) moveOnFriends(m);
    else moveOnSettings(m);
}

void App::moveOnBar(Move m) {
    const moves::BarStep s = moves::bar(place_ == Place::Settings ? 1 : 0, kBarCount, m, screenTakesFocus());
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
        settingsFocus_ = std::clamp(settingsFocus_, 0, screens::setting::Count - 1);
    }
}

int App::tileCount() const { return state_.instead.empty() ? static_cast<int>(order_.size()) : 0; }

bool App::screenTakesFocus() const { return place_ == Place::Settings || tileCount() > 0; }

void App::moveOnFriends(Move m) {
    const moves::Step s = moves::friends(friendFocus_, tileCount(), m);
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
        // Status, which is Appear Offline by another name, and kept.
        settings_.appearOffline = !settings_.appearOffline;
        state_.appearOffline = settings_.appearOffline;
        keepSettings();
        link_->setStatus(statusWord());
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
            const std::string name = trimmed(typedName_);
            typingName_ = false;
            if (textline::ready(name, textline::Kind::Name) && name != name_) {
                name_ = name;
                link_->rename(name);
            }
        } else if (m == Move::Back) {
            typingName_ = false;
        }
        return;
    }
    if ((m == Move::Left || m == Move::Right) &&
        (settingsFocus_ == setting::Colour || settingsFocus_ == setting::Controller)) {
        stepSetting(settingsFocus_, m == Move::Right ? 1 : -1);
        return;
    }
    const moves::Step s = moves::column(settingsFocus_, setting::Count, m, true);
    switch (s.then) {
        case Then::Moved: settingsFocus_ = s.focus; break;
        case Then::ToBar:
        case Then::Back: barFocused_ = true; break;
        case Then::Press: pressSetting(settingsFocus_); break;
        case Then::Edge: break;
    }
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
        case setting::Controller: stepSetting(row, 1); break;
        case setting::FullScreen: setFullScreen(!settings_.fullScreen); break;
        case setting::Server: serverPanel(); break;
        case setting::Credits: {
            overlay_ = Overlay{};
            overlay_.kind = Overlay::Kind::Credits;
            break;
        }
        default: break;
    }
}

// A choice row: one along, stopping at either end, as the console's do.
void App::stepSetting(int row, int by) {
    if (row == screens::setting::Colour) {
        const int now = colourIndex(settings_.colour);
        const int next = std::clamp(now + by, 0, look::kColourCount - 1);
        if (next == now) return;
        settings_.colour = look::colourWord(static_cast<look::Colour>(next));
        keepSettings();
    } else if (row == screens::setting::Controller) {
        const int count = static_cast<int>(padIds_.size());
        const int now = chosenPad();
        const int next = std::clamp(now + by, 0, std::max(0, count - 1));
        if (count == 0 || next == now) return;
        char guid[64] = {};
        SDL_GUIDToString(SDL_GetGamepadGUIDForID(padIds_[static_cast<size_t>(next)]), guid, sizeof guid);
        settings_.controller = guid;
        keepSettings();
    }
}

void App::moveOnFirstRun(Move m) {
    if (first_.connecting) return;
    const moves::Step s = moves::column(first_.focus, 4, m, false);
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
            if (!textline::ready(value, o.fieldKind)) return;
            auto typed = o.typed;
            closeOverlay();
            if (typed) typed(value);
        } else if (m == Move::Back) {
            closeOverlay();
        }
        return;
    }
    if (o.kind == Overlay::Kind::Credits) {
        // Nothing to choose: A or B closes it.
        if (m == Move::Press || m == Move::Back) {
            closeOverlay();
            return;
        }
        const moves::Step s = moves::column(o.panel.focused, screens::creditCount(), m, false);
        if (s.then == Then::Moved) {
            o.panel.focused = s.focus;
            o.panel.top = moves::topFor(s.focus, o.panel.top, screens::creditCount(), parts::kPanelRows);
        }
        return;
    }
    // A line with only a prompt: A goes on, and nothing else does.
    if (o.prompt) {
        if (m == Move::Press) {
            auto then = o.then;
            closeOverlay();
            if (then) then(0);
        }
        return;
    }
    const moves::Step s = moves::column(o.panel.focused, static_cast<int>(o.panel.options.size()), m, false);
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

// ---- The mouse -------------------------------------------------------------

bool App::spotAt(float x, float y, ui::Spot& out, bool overlayOnly) const {
    float cx = 0, cy = 0;
    canvas_.toCanvas(x, y, cx, cy);
    const std::vector<ui::Spot>& spots = canvas_.spots();
    for (auto it = spots.rbegin(); it != spots.rend(); ++it) {
        if (overlayOnly && it->id < parts::spot::kOption) continue;
        if (it->contains(cx, cy)) {
            out = *it;
            return true;
        }
    }
    return false;
}

// Pointing focuses and a click presses. Over a panel only the panel counts, and a click outside it
// is Back. First run's lines take focus only from a click, so the pointer resting on one does not
// move where typing goes. A click on a line's letters puts its caret there.
void App::pointAt(float x, float y, bool click) {
    namespace spot = parts::spot;
    float cx = 0, cy = 0;
    canvas_.toCanvas(x, y, cx, cy);
    ui::Spot s;
    if (overlay_.kind != Overlay::Kind::None) {
        const bool hit = spotAt(x, y, s, true);
        const bool option = hit && s.id >= spot::kOption && s.id < spot::kField;
        if (option && (overlay_.kind == Overlay::Kind::Panel || overlay_.kind == Overlay::Kind::Credits))
            overlay_.panel.focused = s.id - spot::kOption;
        if (!click) return;
        if (overlay_.prompt) {
            move(Move::Press);   // "Click to continue": anywhere
            return;
        }
        if (!hit) move(Move::Back);
        else if (option || (overlay_.kind == Overlay::Kind::Credits && s.id == spot::kPanel)) move(Move::Press);
        else if (overlay_.kind == Overlay::Kind::Field && s.id == spot::kField) clickCaret(s.id, cx, false);
        return;
    }
    if (!spotAt(x, y, s, false)) return;
    if (s.id >= spot::kBar && s.id < spot::kStatus) {
        if (!click || place_ == Place::FirstRun || typingName_) return;
        switchTo(s.id - spot::kBar == 1 ? Place::Settings : Place::Friends);
        barFocused_ = !screenTakesFocus();
        return;
    }
    switch (place_) {
        case Place::FirstRun: {
            if (!click || first_.connecting || s.id < spot::kItem || s.id > spot::kItem + 3) return;
            const int was = first_.focus;
            first_.focus = s.id - spot::kItem;
            if (first_.focus == 3) connect();
            else clickCaret(s.id, cx, first_.focus != was);
            return;
        }
        case Place::Friends:
            if (s.id == spot::kStatus) friendFocus_ = -1;
            else if (s.id >= spot::kItem && s.id < spot::kOption) friendFocus_ = s.id - spot::kItem;
            else return;
            barFocused_ = false;
            if (click) pressFriend();
            return;
        case Place::Settings: {
            if (s.id < spot::kItem || s.id >= spot::kOption) return;
            const int row = s.id - spot::kItem;
            if (typingName_) {
                if (!click) return;
                if (row == screens::setting::Name) {
                    clickCaret(s.id, cx, false);
                    return;
                }
                typingName_ = false;
            }
            settingsFocus_ = row;
            barFocused_ = false;
            if (!click) return;
            pressSetting(row);
            // Pressed into typing: the caret goes where the click was.
            if (typingName_) clickCaret(s.id, cx, false);
            return;
        }
        case Place::Waiting: return;
    }
}

// ---- Typing ----------------------------------------------------------------

bool App::typing() const {
    if (overlay_.kind == Overlay::Kind::Field) return true;
    if (overlay_.kind != Overlay::Kind::None) return false;
    if (place_ == Place::FirstRun) return first_.focus < 3 && !first_.connecting;
    return place_ == Place::Settings && typingName_;
}

std::string* App::typedLine(textline::Kind& kind) {
    if (overlay_.kind == Overlay::Kind::Field) {
        kind = overlay_.fieldKind;
        return &overlay_.field.value;
    }
    if (place_ == Place::Settings && typingName_) {
        kind = textline::Kind::Name;
        return &typedName_;
    }
    if (place_ == Place::FirstRun) {
        switch (first_.focus) {
            case 0: kind = textline::Kind::Address; return &first_.address;
            case 1: kind = textline::Kind::Code; return &first_.code;
            case 2: kind = textline::Kind::Name; return &first_.name;
            default: break;
        }
    }
    return nullptr;
}

// A key or a move holds the caret still, as on the console: it blinks only while it waits.
void App::edit(const std::function<textline::Line(textline::Line, textline::Kind)>& how) {
    textline::Kind kind;
    std::string* line = typedLine(kind);
    if (!line) return;
    textline::Line now = how(textline::Line{*line, caret_}, kind);
    *line = std::move(now.text);
    caret_ = now.caret;
    still_ = 0.0f;
}

void App::typeText(const std::string& text) {
    edit([&text](textline::Line l, textline::Kind kind) { return textline::add(std::move(l), text, kind); });
}

// Left and Right are moves (move), so a held one repeats as every held direction does. These keys
// repeat as the computer repeats them.
bool App::typeKey(const SDL_KeyboardEvent& key) {
    using textline::Kind;
    using textline::Line;
    const auto to = [this](size_t at) {
        edit([at](Line l, Kind kind) { return textline::moveTo(std::move(l), at, kind); });
    };
    switch (key.key) {
        case SDLK_BACKSPACE: edit([](Line l, Kind kind) { return textline::backspace(std::move(l), kind); }); return true;
        case SDLK_DELETE: edit([](Line l, Kind kind) { return textline::forwardDelete(std::move(l), kind); }); return true;
        case SDLK_HOME: to(0); return true;
        case SDLK_END: to(std::string::npos); return true;
#if defined(SDL_PLATFORM_MACOS)
        // A Mac's Home and End.
        case SDLK_LEFT:
        case SDLK_RIGHT:
            if (!(key.mod & SDL_KMOD_GUI)) return false;
            to(key.key == SDLK_LEFT ? 0 : std::string::npos);
            return true;
#endif
        case SDLK_V:
            if (!(key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI))) return false;
            if (char* clip = SDL_GetClipboardText()) {
                typeText(clip);
                SDL_free(clip);
            }
            return true;
        default: return false;
    }
}

void App::caretToEnd() {
    textline::Kind kind;
    const std::string* line = typedLine(kind);
    caret_ = line ? line->size() : 0;
}

void App::clickCaret(int spot, float x, bool fresh) {
    if (const std::optional<size_t> at = canvas_.caretAt(spot, x))
        edit([at](textline::Line l, textline::Kind kind) { return textline::moveTo(std::move(l), *at, kind); });
    else if (fresh)
        caretToEnd();
}

// ---- What happens ------------------------------------------------------------

// First run's Connect: what is missing is focused, and a short code is said, as the console says it.
void App::connect() {
    const std::string name = trimmed(first_.name);
    if (!textline::ready(first_.address, textline::Kind::Address)) {
        first_.focus = 0;
        caretToEnd();
        return;
    }
    if (!textline::ready(first_.code, textline::Kind::Code)) {
        first_.focus = 1;
        caretToEnd();
        notice_.say(kCodeLength, Tone::Problem);
        return;
    }
    if (!textline::ready(name, textline::Kind::Name)) {
        first_.focus = 2;
        caretToEnd();
        return;
    }
    notice_.clear();
    first_.focus = 3;
    first_.connecting = true;
    pairingLeft_ = kPairingSeconds;
    link_->pair(first_.address, first_.code, name);
}

void App::answerInvite(const linkup::Friend& f) {
    parts::Panel p;
    p.title = linkup::joinQuestion(f.name);
    p.detail = f.game;
    p.options = {"Join", "Decline"};
    p.width = parts::kLinkUpPanelWidth;
    openPanel(p, [this, f](int answer) {
        if (answer == 0) join(f);
        else if (answer == 1) decline(f);
    });
    overlay_.answering = f.name;
}

void App::join(const linkup::Friend& f) {
    if (notice_.text == linkup::invitedYou(f.name)) notice_.clear();
    joinHost_ = f.name;
    joinGame_ = f.game;
    joinLeft_ = false;
    place_ = Place::Waiting;
    link_->join(f.invite);
}

void App::decline(const linkup::Friend& f) {
    if (notice_.text == linkup::invitedYou(f.name)) notice_.clear();
    const std::string invite = f.invite;
    tookInviteEnded(invite);
    link_->decline(invite);
}

// One row for the server, saying which, and the rest in a panel (pressLinkUpServer). This app runs
// no server, so the console's Run your own is not there.
void App::serverPanel() {
    parts::Panel p;
    p.title = address_;
    p.options = {"Change server", "Leave"};
    p.width = parts::kLinkUpPanelWidth;
    openPanel(p, [this](int answer) {
        if (answer == 0) askAddress();
        if (answer == 1) leave();
    });
}

// linkupRetypeAddress.
void App::askAddress() {
    overlay_ = Overlay{};
    overlay_.kind = Overlay::Kind::Field;
    overlay_.field.title = "Server address";
    overlay_.field.value = address_;
    overlay_.field.placeholder = "name.example.net:7845";
    overlay_.fieldKind = textline::Kind::Address;
    overlay_.typed = [this](const std::string& address) {
        if (address != address_) link_->retype(address);
    };
    caretToEnd();
    still_ = 0.0f;
}

// linkupAskCode: another server's code, with its address under the title.
void App::askCode(const std::string& address) {
    overlay_ = Overlay{};
    overlay_.kind = Overlay::Kind::Field;
    overlay_.field.title = "Code";
    overlay_.field.hint = address;
    overlay_.field.placeholder = "XXXX-XXXX-XXXX";
    overlay_.fieldKind = textline::Kind::Code;
    overlay_.typed = [this, address](const std::string& code) {
        changing_ = true;
        notice_.say("Pairing\xE2\x80\xA6", Tone::Busy);
        link_->pair(address, code, name_);
    };
    caretToEnd();
    still_ = 0.0f;
}

// pressLinkUpLeave: starting on the harmless one.
void App::leave() {
    parts::Panel p;
    p.title = "Leave " + address_ + "?";
    p.options = {"Leave", "Cancel"};
    p.focused = 1;
    p.width = parts::kLinkUpPanelWidth;
    openPanel(p, [this](int answer) {
        if (answer == 0) link_->leave();
    });
}

void App::toFirstRun() {
    place_ = Place::FirstRun;
    barFocused_ = false;
    overlay_ = Overlay{};
    paired_ = false;
    address_.clear();
    state_.friends.clear();
    state_.instead.clear();
    state_.paired = false;
    order_.clear();
    friendFocus_ = -1;
    typingName_ = false;
    first_ = screens::FirstRunView{};
    // The name stays: it is the friend's own, whichever server.
    first_.name = name_;
    caretToEnd();
    settingsFocus_ = 0;
}

void App::openPanel(parts::Panel p, std::function<void(int)> then) {
    overlay_ = Overlay{};
    overlay_.kind = Overlay::Kind::Panel;
    overlay_.panel = std::move(p);
    overlay_.then = std::move(then);
}

// A line until A, over whatever it ended on: nobody may be watching by now.
void App::openPrompt(const std::string& title, const std::string& detail, const std::string& after) {
    parts::Panel p;
    p.title = title;
    p.detail = detail;
    p.width = parts::kLinkUpPanelWidth;
    openPanel(p, nullptr);
    overlay_.prompt = true;
    overlay_.after = after;
}

void App::closeOverlay() { overlay_ = Overlay{}; }

void App::setFullScreen(bool on) {
    settings_.fullScreen = on;
    SDL_SetWindowFullscreen(window_, on);
    keepSettings();
}

void App::keepSettings() { guest::keepSettings(folder_, settings_); }

// ---- The controllers ----------------------------------------------------------

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
