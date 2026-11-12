// A game, the friend's side of its connection: what the console's LinkUp program does once a friend
// has joined (conductor.cpp in CabinetOS: startFriend, onSignalBox, applySignal, tick, onFast and
// onControl). The server's half, the hello and the sealed boxes, is guest::Link's; this is the
// connection between the two computers, the stream on it, and what the two say on its data channels.
//
// IT RUNS ON A THREAD OF ITS OWN, with GLib's main loop, as the console's LinkUp program runs on its
// own: webrtcbin and rtc.cpp hand everything to that loop (Rtc::onMain). Everything the window asks
// is passed to the loop the same way, and what happened comes back as events, which the window takes
// each frame. `wake` is called after each one, from whichever thread made it.
//
// ON THE DATA CHANNELS (rtc.h): the host makes both. On "fast", this app sends its pad's state ('P')
// and a beat ('B') every 250 ms, and answers the host's beat ('b'); the host's rumble ('R') comes
// back on it, and a request for a full picture ('K') goes to the host when packets arrive and no
// picture comes out. On "control": the host's picture on or off, "bye", and "p:" with a word for the
// screens, as JSON, both ways.
//
// A NEW CONNECTION is the host's to make: its connection details come under a higher number, and the
// old stream goes and a new one starts with them. Nothing heard for 2 s is a drop.
//
// No GStreamer here: the window includes this, and where the game is not built (-DTWOPSTART_GAME=OFF),
// none.cpp stands in and can play nothing.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "linkup_wire.h"

namespace game {

// One picture's two planes, NV12: the brightness, then the colour at half size, each row `pitch`
// bytes apart. Colours are BT.601, limited range, as the console states after its decoder.
struct Planes {
    int width = 0;
    int height = 0;
    const uint8_t* y = nullptr;
    int yPitch = 0;
    const uint8_t* uv = nullptr;
    int uvPitch = 0;
};

class Game {
public:
    struct Event {
        enum class Kind {
            Connected,   // the two computers are connected, by `text`: direct or relay
            Failed,      // never connected, for `text`: no_path, relay_limit or other
            Picture,     // the host's picture is `on`, or not (something else is on its screen)
            Peer,        // the host's word for the screens: `text` is its JSON
            Lost,        // nothing heard from the host for 2 s
            Back,        // heard again
            Gone,        // the host said goodbye
            Rumble,      // the host's game rumbles player two: `low`, `high`
        };
        Kind kind = Kind::Connected;
        std::string text;
        bool on = false;
        uint16_t low = 0, high = 0;
    };

    // Where connection details and the count of how games connect go: the server, through Link.
    struct Server {
        std::function<void(int generation, const std::string& details)> signal;
        std::function<void(const std::string& path, const std::string& reason)> report;
    };

    explicit Game(std::function<void()> wake);
    ~Game();
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    // Whether this computer can play: GStreamer has webrtcbin and everything a stream needs, the
    // graphics chip's H.264 decoder included. Waits for GStreamer to start, the first time.
    bool canPlay();
    // What it lacks, by GStreamer's names, and GStreamer's plugins that would not load: for
    // --check-game.
    std::vector<std::string> lacking();
    std::vector<std::string> unloadable();

    // The window's thread. Each returns at once.
    //
    // A game is joined. Nothing connects until `ready`.
    void begin(Server server);
    // The server's STUN list, the relay, or why there is none (Link's Ready).
    void ready(std::vector<std::string> stun, std::string turn, std::string relayReason);
    // The host's connection details (Link's Signal).
    void signal(int generation, const std::string& details);
    void pad(const linkupwire::Pad& pad);
    // A word for the host's screens (linkupTellPeer).
    void tell(const std::string& word);
    // The game is over here. With `bye`, the host is told first, as when this app quits.
    void end(bool bye);

    std::vector<Event> take();
    // The newest picture the window has not drawn, given to `use` while it is held.
    bool takePicture(const std::function<void(const Planes&)>& use);

private:
    class Loop;
    std::unique_ptr<Loop> loop_;
};

}  // namespace game
