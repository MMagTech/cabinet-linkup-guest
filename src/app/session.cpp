// A game, in 2P Start's window (app.h): the friend's side of the console's linkupsession.cpp, each
// part named for the function it follows there, so the two can be read side by side. The host's
// words for the screens, a drop, the pause, leaving, the friend's pad, and the picture.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <json-c/json.h>

#include "app/app.h"
#include "jsonutil.h"

namespace {

using Move = moves::Move;
using Tone = parts::Tone;

// linkupsession.cpp's clocks.
constexpr float kDropEndsSeconds = 60.0f;
constexpr float kPressEverySeconds = 2.0f;
constexpr float kPadKeepAliveSeconds = 0.1f;
constexpr Uint32 kRumbleMs = 300;

// What a sensor reads now, in SDL's units and axes (the console's tilt.h): false, with zeros, when the
// controller has no such sensor or it is off.
bool readSensor(SDL_Gamepad* pad, SDL_SensorType type, float out[3]) {
    out[0] = out[1] = out[2] = 0.0f;
    return pad && SDL_GamepadSensorEnabled(pad, type) && SDL_GetGamepadSensorData(pad, type, out, 3);
}

// linkuppad::differs: anything but `seq`.
bool differs(const linkupwire::Pad& a, const linkupwire::Pad& b) {
    return a.senses != b.senses || a.buttons != b.buttons || std::memcmp(a.axes, b.axes, sizeof a.axes) != 0 ||
           std::memcmp(a.accel, b.accel, sizeof a.accel) != 0 || std::memcmp(a.gyro, b.gyro, sizeof a.gyro) != 0;
}

}  // namespace

// ---- What the game says --------------------------------------------------------

void App::fromGame() {
    for (game::Game::Event& e : game_->take()) {
        // A game left or ended here is over: what its connection says after that is not for anybody.
        if (!session_ || place_ != Place::Game) continue;
        using Kind = game::Game::Event::Kind;
        switch (e.kind) {
            case Kind::Connected: connected(); break;
            case Kind::Failed: connectFailed(e.text); break;
            case Kind::Picture: friendPicture(e.on); break;
            case Kind::Peer: {
                json_object* p = jsonutil::parse(e.text);
                if (!p) break;
                fromPeer(jsonutil::str(p, "t"), jsonutil::str(p, "game"));
                json_object_put(p);
                break;
            }
            case Kind::Lost: peerLost(); break;
            case Kind::Back: peerBack(); break;
            case Kind::Gone: peerEndedSession(); break;
            case Kind::Rumble:
                if (SDL_Gamepad* pad = playingPad()) SDL_RumbleGamepad(pad, e.low, e.high, kRumbleMs);
                break;
        }
    }
}

// linkupConnected, the friend's side: the picture can come from now on. Back from a drop on a new
// connection, everything already stands.
void App::connected() {
    if (session_->streamUp) return;
    session_->streamUp = true;
}

// linkupConnectFailed. The friend's side only ever hears that it never connected.
void App::connectFailed(const std::string& reason) {
    const std::string host = session_->host;
    if (!session_->id.empty()) link_->end(session_->id);
    sessionClosed();
    openPrompt(linkup::cantConnectTo(host), reason == "relay_limit" ? "The relay has reached its limit" : "", "to continue");
}

// linkupFriendPicture.
void App::friendPicture(bool on) {
    Session& s = *session_;
    s.showsPicture = on;
    if (on) {
        // The game is back, and whatever was said about why it was gone is over.
        s.pausedThere = false;
        s.resumes = false;
        if (!s.pausedHere) s.shows = Shows::Picture;
    } else if (!s.pausedHere && s.shows == Shows::Picture) {
        showWaiting(linkup::waitingFor(s.host), s.game);
    }
}

// linkupFromPeer, the friend's side.
void App::fromPeer(const std::string& word, const std::string& game) {
    Session& s = *session_;
    if (word == "slow") {
        notice_.say(linkup::kTooSlow, Tone::Problem);
    } else if (word == "resume") {
        s.pausedHere = false;
        if (overlay_.kind != Overlay::Kind::None) closeOverlay();
    } else if (word == "paused") {
        s.pausedThere = true;
        if (!s.pausedHere) {
            showWaiting(linkup::pausedBy(s.host), s.game);
            s.resumes = true;
        }
    } else if (word == "resumed") {
        s.pausedThere = false;
        s.resumes = false;
        if (!s.pausedHere && !s.showsPicture) showWaiting(linkup::waitingFor(s.host), s.game);
    } else if (word == "starting") {
        // With its name: a session's next game is not the one the invite named.
        if (!game.empty()) s.game = game;
        showWaiting(linkup::waitingFor(s.host), s.game);
    } else if (word == "quit") {
        showWaiting(linkup::waitingFor(s.host), "");
    } else if (word == "ended") {
        peerEndedSession();
    } else if (word == "idle") {
        idlePause();
    }
}

// linkupIdlePause, the friend's side: the host's clock said ten minutes with no press from either.
void App::idlePause() {
    if (overlay_.kind != Overlay::Kind::None) return;
    session_->pausedHere = true;
    openPrompt(linkup::kStillPlaying, "", "to resume");
    overlay_.then = [this](int) {
        if (!session_) return;
        session_->pausedHere = false;
        game_->tell("resume");
    };
}

// linkupPeerLost: under about two seconds shows nothing. Then this says so, and the words change
// back if the host returns. After a minute, they left.
void App::peerLost() {
    Session& s = *session_;
    if (s.lost) return;
    s.lost = true;
    s.lostFor = 0.0f;
    closeOverlay();
    s.shows = Shows::Reconnecting;
    s.title = linkup::reconnectingTo(s.host);
    s.under.clear();
    s.resumes = false;
}

// linkupPeerBack.
void App::peerBack() {
    Session& s = *session_;
    if (!s.lost) return;
    s.lost = false;
    if (s.showsPicture) s.shows = Shows::Picture;
    else showWaiting(linkup::waitingFor(s.host), s.game);
}

// linkupPeerEndedSession: the host ended it, said goodbye, or the server says it is over.
void App::peerEndedSession() {
    const std::string host = session_->host;
    if (!session_->id.empty()) link_->end(session_->id);
    sessionClosed();
    openPrompt(linkup::endedSession(host), "", "to continue");
}

// linkupSessionClosed: back on the list.
void App::sessionClosed() {
    if (SDL_Gamepad* pad = playingPad()) SDL_RumbleGamepad(pad, 0, 0, 0);
    sense(nullptr);
    game_->end(false);
    session_.reset();
    place_ = Place::Friends;
    barFocused_ = tileCount() == 0;
    closeOverlay();
}

void App::showWaiting(const std::string& title, const std::string& under) {
    Session& s = *session_;
    s.shows = Shows::Waiting;
    s.title = title;
    s.under = under;
    s.resumes = false;
}

// ---- The friend's own presses -----------------------------------------------------

// A move on a game's own screen: B leaves, and A resumes a game the host paused (kResumeBoth).
void App::moveInGame(Move m) {
    if (!session_) return;
    if (m == Move::Back) {
        askLeave();
    } else if (m == Move::Press && session_->resumes) {
        game_->tell("resume");
    }
}

// linkupAskLeave: over the game, Paused, which stops it for both; with no game to pause, only the
// leaving, starting on the harmless one.
void App::askLeave() {
    if (!session_) return;
    if (session_->shows != Shows::Picture) {
        parts::Panel p;
        p.title = linkup::leaveQuestion(session_->host);
        p.options = {"Leave", "Cancel"};
        p.focused = 1;
        p.width = parts::kLinkUpPanelWidth;
        openPanel(p, [this](int answer) {
            if (answer == 0) friendLeaves();
        });
        return;
    }
    friendAsksPause();
}

// linkupFriendAsksPause: the game stops for both while this is up; the host's console says so.
void App::friendAsksPause() {
    session_->pausedHere = true;
    game_->tell("pause");
    parts::Panel p;
    p.title = "Paused";
    p.options = {"Resume", "Leave"};
    p.width = parts::kLinkUpPanelWidth;
    openPanel(p, [this](int answer) {
        if (!session_) return;
        session_->pausedHere = false;
        if (answer == 1) {
            friendLeaves();
            return;
        }
        game_->tell("resume");
    });
}

// linkupFriendLeaves. Before the join is answered there is no session to end yet: it is ended when
// the answer comes (tookJoined).
void App::friendLeaves() {
    if (!session_) return;
    if (session_->id.empty()) {
        session_->left = true;
        game_->end(false);
        place_ = Place::Friends;
        barFocused_ = tileCount() == 0;
        return;
    }
    game_->tell("leave");
    link_->end(session_->id);
    sessionClosed();
}

// takeLinkUpEvent: while the picture is up with nothing over it, the controller is the game's, and
// so are the keys, apart from the pause. The pause: both sticks in, as in every game, the Guide
// button, or Esc on the keyboard.
bool App::takeGameEvent(const SDL_Event& e) {
    if (!session_ || place_ != Place::Game || !pictureBehind() || overlay_.kind != Overlay::Kind::None) return false;
    Session& s = *session_;
    switch (e.type) {
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
            setHands(parts::Hands::Controller);
            const bool down = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
            if (down) s.pressHere = true;
            const auto b = static_cast<SDL_GamepadButton>(e.gbutton.button);
            if (b == SDL_GAMEPAD_BUTTON_LEFT_STICK) s.l3 = down;
            if (b == SDL_GAMEPAD_BUTTON_RIGHT_STICK) s.r3 = down;
            if (down && s.l3 && s.r3) {
                s.l3 = s.r3 = false;
                askLeave();
            } else if (b == SDL_GAMEPAD_BUTTON_GUIDE && !down) {
                askLeave();
            }
            return true;
        }
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (std::abs(static_cast<int>(e.gaxis.value)) > idle::kAxisDeadzone) s.pressHere = true;
            return true;
        case SDL_EVENT_KEY_DOWN:
            if (e.key.key == SDLK_ESCAPE && !e.key.repeat) askLeave();
            return true;
        case SDL_EVENT_KEY_UP:
            return true;
        default:
            return false;
    }
}

// ---- The clocks, and the pad ---------------------------------------------------------

// tickLinkUpSession, the friend's side: presses here keep the host's idle clock from running out,
// and a drop that lasts a minute is the host gone.
void App::tickSession(float dt) {
    if (!session_ || place_ != Place::Game) return;
    Session& s = *session_;
    s.pressAge += dt;
    if (s.pressHere && s.pressAge >= kPressEverySeconds && s.streamUp) {
        s.pressHere = false;
        s.pressAge = 0.0f;
        game_->tell("press");
    }
    if (s.lost) {
        s.lostFor += dt;
        if (s.lostFor >= kDropEndsSeconds) {
            const std::string host = s.host;
            if (!s.id.empty()) link_->end(s.id);
            sessionClosed();
            openPrompt(linkup::left(host), "", "to continue");
        }
    }
    // The swap wait is off while the picture is all there is, and on again for anything drawn over it.
    if (live() != liveDrawn_) {
        liveDrawn_ = live();
        SDL_SetRenderVSync(renderer_, liveDrawn_ ? 0 : 1);
    }
}

// linkupSendPad and linkuppad::read: the controller chosen in Settings, to the host, whenever it
// changes and every tenth of a second: its presses, its sticks, and its tilt, with the motion sensors
// it has. Only the controller travels: the keyboard and the mouse stay here. The Guide button never
// goes: it is this computer's. Asleep (something over the picture, or another program in front, where
// SDL still reads the pad for the idle rule, main.cpp), it is let go and held still: no button,
// sticks centred, no turning, and tilted as the last state sent had it.
void App::sendPad(float dt) {
    if (!session_ || place_ != Place::Game || !session_->streamUp) return;
    Session& s = *session_;
    linkupwire::Pad pad;
    const bool inFront = (SDL_GetWindowFlags(window_) & SDL_WINDOW_INPUT_FOCUS) != 0;
    const bool asleep = !pictureBehind() || overlay_.kind != Overlay::Kind::None || !inFront;
    SDL_Gamepad* gp = playingPad();
    sense(gp);
    float accel[3], gyro[3];
    const bool hasAccel = readSensor(gp, SDL_SENSOR_ACCEL, accel);
    const bool hasGyro = readSensor(gp, SDL_SENSOR_GYRO, gyro);
    pad.senses = static_cast<uint8_t>((hasAccel ? linkupwire::kSenseAccel : 0) | (hasGyro ? linkupwire::kSenseGyro : 0));
    if (gp && !asleep) {
        for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b) {
            if (b == SDL_GAMEPAD_BUTTON_GUIDE) continue;
            if (SDL_GetGamepadButton(gp, static_cast<SDL_GamepadButton>(b))) pad.buttons |= 1u << b;
        }
        for (int a = 0; a < SDL_GAMEPAD_AXIS_COUNT; ++a) pad.axes[a] = SDL_GetGamepadAxis(gp, static_cast<SDL_GamepadAxis>(a));
        std::memcpy(pad.accel, accel, sizeof pad.accel);
        std::memcpy(pad.gyro, gyro, sizeof pad.gyro);
    } else if (hasAccel) {
        const bool held = (s.sent.senses & linkupwire::kSenseAccel) != 0;
        std::memcpy(pad.accel, held ? s.sent.accel : accel, sizeof pad.accel);
    }
    s.padAge += dt;
    if (!differs(pad, s.sent) && s.padAge < kPadKeepAliveSeconds) return;
    s.padAge = 0.0f;
    pad.seq = ++s.seq;
    s.sent = pad;
    game_->pad(pad);
}

// linkuppad::senseHere: the playing controller's accelerometer and gyroscope on while a game's stream
// is up, and off after it, or when another controller plays.
void App::sense(SDL_Gamepad* pad) {
    if (pad == sensing_) return;
    for (const bool on : {false, true}) {
        SDL_Gamepad* which = on ? pad : sensing_;
        if (!which) continue;
        SDL_SetGamepadSensorEnabled(which, SDL_SENSOR_ACCEL, on);
        SDL_SetGamepadSensorEnabled(which, SDL_SENSOR_GYRO, on);
    }
    sensing_ = pad;
    if (pad) {
        const bool accel = SDL_GamepadHasSensor(pad, SDL_SENSOR_ACCEL), gyro = SDL_GamepadHasSensor(pad, SDL_SENSOR_GYRO);
        const char* name = SDL_GetGamepadName(pad);
        std::fprintf(stderr, "[game] %s plays, with %s\n", name ? name : "a controller",
                     accel && gyro ? "an accelerometer and a gyroscope" : accel ? "an accelerometer" : gyro ? "a gyroscope" : "no motion sensors");
    }
}

SDL_Gamepad* App::playingPad() const {
    if (padIds_.empty()) return nullptr;
    auto it = pads_.find(padIds_[static_cast<size_t>(chosenPad())]);
    return it == pads_.end() ? nullptr : it->second;
}

// ---- The picture --------------------------------------------------------------

// linkupPictureBehind.
bool App::pictureBehind() const { return place_ == Place::Game && session_ && session_->shows == Shows::Picture; }

// linkupPictureOnly.
bool App::live() const { return pictureBehind() && overlay_.kind == Overlay::Kind::None && !notice_.showing(); }

// The newest picture into the texture the window draws, BT.601 limited range, as the console states
// it after its decoder. A new size makes a new texture.
void App::takePicture() {
    fresh_ = false;
    game_->takePicture([this](const game::Planes& p) {
        float w = 0, h = 0;
        if (picture_) SDL_GetTextureSize(picture_, &w, &h);
        if (!picture_ || static_cast<int>(w) != p.width || static_cast<int>(h) != p.height) {
            if (picture_) SDL_DestroyTexture(picture_);
            SDL_PropertiesID props = SDL_CreateProperties();
            SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, SDL_PIXELFORMAT_NV12);
            SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER, SDL_TEXTUREACCESS_STREAMING);
            SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, p.width);
            SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, p.height);
            SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER, SDL_COLORSPACE_BT601_LIMITED);
            picture_ = SDL_CreateTextureWithProperties(renderer_, props);
            SDL_DestroyProperties(props);
            if (!picture_) {
                SDL_Log("2pstart: no texture for the picture: %s", SDL_GetError());
                return;
            }
            SDL_SetTextureScaleMode(picture_, SDL_SCALEMODE_LINEAR);
        }
        SDL_UpdateNVTexture(picture_, nullptr, p.y, p.yPitch, p.uv, p.uvPitch);
        fresh_ = true;
    });
}

// The host's picture, as large as the window allows, its shape kept, black around it.
void App::drawPicture() {
    if (!picture_) return;
    int ow = 0, oh = 0;
    SDL_GetRenderOutputSize(renderer_, &ow, &oh);
    float w = 0, h = 0;
    SDL_GetTextureSize(picture_, &w, &h);
    if (w <= 0 || h <= 0 || ow <= 0 || oh <= 0) return;
    const float scale = std::min(static_cast<float>(ow) / w, static_cast<float>(oh) / h);
    SDL_FRect at{(static_cast<float>(ow) - w * scale) * 0.5f, (static_cast<float>(oh) - h * scale) * 0.5f, w * scale, h * scale};
    SDL_RenderTexture(renderer_, picture_, nullptr, &at);
}
