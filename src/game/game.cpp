// A game, the friend's side of its connection (game.h). Each part names the function of CabinetOS's
// conductor.cpp it follows, so the two can be read side by side.

#include "game/game.h"

#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>

#include <json-c/json.h>
#include <openssl/rand.h>

#include "game/stream.h"
#include "jsonutil.h"

namespace game {

namespace {

// What goes on the fast channel beside a pad's state and a rumble (conductor.cpp).
constexpr uint8_t kTagBeat = 'B';
constexpr uint8_t kTagBeatBack = 'b';
constexpr uint8_t kTagFullPicture = 'K';

// The heartbeat, how long with nothing heard counts as a drop, and how long a connection has to
// come at all (conductor.cpp, tuned on the testers' connections).
constexpr int64_t kBeatMs = 250;
constexpr int64_t kDropMs = 2000;
constexpr int64_t kConnectLimitMs = 30000;

bool hasElement(const char* name) {
    GstElementFactory* f = gst_element_factory_find(name);
    if (!f) return false;
    gst_object_unref(f);
    return true;
}

// firstStunThatAnswers: the first STUN server that answers, in the order the owner set. webrtcbin
// takes one. A binding request with half a second for each, on a thread of its own.
std::string firstStunThatAnswers(const std::vector<std::string>& list) {
    for (const std::string& entry : list) {
        const size_t colon = entry.rfind(':');
        if (colon == std::string::npos) continue;
        std::string host = entry.substr(0, colon);
        if (host.size() > 2 && host.front() == '[' && host.back() == ']') host = host.substr(1, host.size() - 2);
        addrinfo hints{};
        hints.ai_socktype = SOCK_DGRAM;
        addrinfo* res = nullptr;
        if (getaddrinfo(host.c_str(), entry.substr(colon + 1).c_str(), &hints, &res) != 0) continue;
        bool answered = false;
        for (addrinfo* a = res; a && !answered; a = a->ai_next) {
            const int fd = socket(a->ai_family, a->ai_socktype, 0);
            if (fd < 0) continue;
            fcntl(fd, F_SETFD, FD_CLOEXEC);
            uint8_t request[20] = {0, 1, 0, 0, 0x21, 0x12, 0xA4, 0x42};
            RAND_bytes(request + 8, 12);
            if (connect(fd, a->ai_addr, a->ai_addrlen) == 0 && send(fd, request, sizeof request, 0) == sizeof request) {
                pollfd p{fd, POLLIN, 0};
                if (poll(&p, 1, 500) == 1) {
                    uint8_t reply[256];
                    const ssize_t n = recv(fd, reply, sizeof reply, 0);
                    answered = n >= 20 && reply[0] == 1 && reply[1] == 1 && std::memcmp(reply + 8, request + 8, 12) == 0;
                }
            }
            close(fd);
        }
        freeaddrinfo(res);
        if (answered) return entry;
    }
    return {};
}

}  // namespace

// Everything of a game that lives on the loop's thread, and the thread itself.
class Game::Loop {
public:
    explicit Loop(std::function<void()> wake);
    ~Loop();

    bool canPlay();
    void begin(Server server);
    void ready(std::vector<std::string> stun, std::string turn, std::string relayReason);
    void signal(int generation, const std::string& details);
    void pad(const linkupwire::Pad& pad);
    void tell(const std::string& word);
    void end(bool bye);
    std::vector<Event> take();
    bool takePicture(const std::function<void(const Planes&)>& use) { return pictures_.take(use); }

private:
    struct Session;

    void run();
    void post(Event e);
    // The loop's own.
    void startStream();
    void applySignal(const std::string& details);
    void tick();
    void onFast(const uint8_t* data, size_t len);
    void onControl(const std::string& text);
    void stopSession(bool bye);
    Rtc::Hooks makeHooks();

    std::function<void()> wake_;
    Pictures pictures_;
    std::thread thread_;
    std::mutex startMu_;
    std::condition_variable startCv_;
    bool started_ = false;
    bool canPlay_ = false;
    GMainLoop* loop_ = nullptr;

    std::mutex eventsMu_;
    std::vector<Event> events_;

    // The loop's alone.
    std::unique_ptr<Session> session_;
    // Read on the data channel's thread, which answers a beat at once (Conductor::onFast).
    std::atomic<Rtc*> live_{nullptr};
    guint tickSource_ = 0;
    uint64_t sessions_ = 0;
};

// Conductor::Game, the friend's part of it.
struct Game::Loop::Session {
    uint64_t id = 0;
    Server server;
    int generation = 1;
    bool haveReady = false;
    bool streamStarted = false;
    bool connected = false;
    bool lost = false;
    std::string stun;
    std::string turn;
    std::string relayReason;
    int64_t startedMs = 0;
    std::vector<std::string> waiting;
    std::unique_ptr<Stream> stream;
    int64_t lastBeatMs = 0;
    int64_t lastRttLogMs = 0;
    std::vector<int> rtt;
    std::string path;
};

Game::Loop::Loop(std::function<void()> wake) : wake_(std::move(wake)) {
    pictures_.setWake(wake_);
    thread_ = std::thread([this]() { run(); });
}

Game::Loop::~Loop() {
    {
        std::unique_lock<std::mutex> lk(startMu_);
        startCv_.wait(lk, [this]() { return started_; });
    }
    Rtc::onMain([this]() {
        stopSession(false);
        g_main_loop_quit(loop_);
    });
    thread_.join();
}

void Game::Loop::run() {
    gst_init(nullptr, nullptr);
    const bool can = hasElement("webrtcbin") && hasElement("rtph264depay") && hasElement("h264parse") &&
                     Stream::canDecodeH264() && hasElement("rtpopusdepay") && hasElement("opusdec") && hasElement("appsink");
    GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
    tickSource_ = g_timeout_add(
        50,
        [](gpointer self) -> gboolean {
            static_cast<Loop*>(self)->tick();
            return G_SOURCE_CONTINUE;
        },
        this);
    {
        std::lock_guard<std::mutex> lk(startMu_);
        loop_ = loop;
        canPlay_ = can;
        started_ = true;
    }
    startCv_.notify_all();
    if (!can) std::fprintf(stderr, "[game] GStreamer lacks what a game needs: webrtcbin, H.264 and Opus\n");
    g_main_loop_run(loop);
    g_source_remove(tickSource_);
    g_main_loop_unref(loop);
}

bool Game::Loop::canPlay() {
    std::unique_lock<std::mutex> lk(startMu_);
    startCv_.wait(lk, [this]() { return started_; });
    return canPlay_;
}

void Game::Loop::post(Event e) {
    {
        std::lock_guard<std::mutex> lk(eventsMu_);
        events_.push_back(std::move(e));
    }
    if (wake_) wake_();
}

std::vector<Game::Event> Game::Loop::take() {
    std::lock_guard<std::mutex> lk(eventsMu_);
    std::vector<Event> out;
    out.swap(events_);
    return out;
}

// ---- Asked by the window ------------------------------------------------------

// cmdJoin's answer and prepareConnection: the session, waiting on the ways through.
void Game::Loop::begin(Server server) {
    Rtc::onMain([this, server = std::move(server)]() {
        stopSession(false);
        session_ = std::make_unique<Session>();
        session_->id = ++sessions_;
        session_->server = server;
        session_->startedMs = Rtc::nowMs();
    });
}

void Game::Loop::ready(std::vector<std::string> stun, std::string turn, std::string relayReason) {
    Rtc::onMain([this, stun = std::move(stun), turn = std::move(turn), relayReason = std::move(relayReason)]() {
        if (!session_) return;
        session_->turn = turn;
        session_->relayReason = relayReason;
        const uint64_t id = session_->id;
        std::thread([this, id, stun]() {
            const std::string first = firstStunThatAnswers(stun);
            Rtc::onMain([this, id, first]() {
                if (!session_ || session_->id != id) return;
                session_->stun = first;
                session_->haveReady = true;
                startStream();
            });
        }).detach();
    });
}

// onSignalBox, after the box is opened (Link): a new connection for this game from a host that lost
// the old one, or more for the one there is.
void Game::Loop::signal(int generation, const std::string& details) {
    Rtc::onMain([this, generation, details]() {
        if (!session_) return;
        Session& s = *session_;
        if (generation > s.generation && s.stream) {
            s.generation = generation;
            live_ = nullptr;
            s.stream->stop();
            s.stream.reset();
            s.streamStarted = false;
            s.waiting.clear();
            s.waiting.push_back(details);
            startStream();
            return;
        }
        if (generation < s.generation) return;
        s.generation = generation;
        applySignal(details);
    });
}

void Game::Loop::pad(const linkupwire::Pad& pad) {
    Rtc::onMain([this, pad]() {
        if (Rtc* r = live_.load()) r->sendFast(&pad, sizeof pad);
    });
}

void Game::Loop::tell(const std::string& word) {
    json_object* inner = json_object_new_object();
    jsonutil::set(inner, "t", word);
    const std::string text = "p:" + jsonutil::text(inner);
    json_object_put(inner);
    Rtc::onMain([this, text]() {
        if (Rtc* r = live_.load()) r->sendControl(text);
    });
}

void Game::Loop::end(bool bye) {
    Rtc::onMain([this, bye]() { stopSession(bye); });
}

// ---- The loop's own ------------------------------------------------------------

// startFriend.
void Game::Loop::startStream() {
    if (!session_ || session_->streamStarted || !session_->haveReady) return;
    Session& s = *session_;
    auto stream = std::make_unique<Stream>(
        makeHooks(),
        [this]() {
            const uint8_t ask = kTagFullPicture;
            if (Rtc* r = live_.load()) r->sendFast(&ask, 1);
        },
        pictures_);
    Rtc::Paths paths;
    paths.stun = s.stun;
    paths.turn = s.turn;
    if (!stream->start(paths)) {
        Event e;
        e.kind = Event::Kind::Failed;
        e.text = "other";
        post(std::move(e));
        return;
    }
    s.stream = std::move(stream);
    live_ = s.stream->rtc();
    s.streamStarted = true;
    std::vector<std::string> waiting;
    waiting.swap(s.waiting);
    for (const std::string& d : waiting) applySignal(d);
}

void Game::Loop::applySignal(const std::string& details) {
    Session& s = *session_;
    Rtc* r = live_.load();
    if (!r || !s.streamStarted) {
        s.waiting.push_back(details);
        return;
    }
    json_object* d = jsonutil::parse(details);
    if (!d) return;
    const std::string sdp = jsonutil::str(d, "sdp");
    if (!sdp.empty()) {
        r->onSignal(jsonutil::str(d, "kind"), sdp, 0);
    } else {
        const std::string ice = jsonutil::str(d, "ice");
        if (!ice.empty()) r->onSignal("ice", ice, static_cast<unsigned>(jsonutil::num(d, "m")));
    }
    json_object_put(d);
}

// makeHooks.
Rtc::Hooks Game::Loop::makeHooks() {
    Rtc::Hooks hooks;
    hooks.signal = [this](const std::string& details) {
        if (session_ && session_->server.signal) session_->server.signal(session_->generation, details);
    };
    hooks.connected = [this](const std::string& path) {
        if (!session_) return;
        session_->connected = true;
        session_->path = path;
        std::fprintf(stderr, "[game] connected: %s\n", path.c_str());
        Event e;
        e.kind = Event::Kind::Connected;
        e.text = path;
        post(std::move(e));
        if (session_->server.report) session_->server.report(path, "none");
    };
    hooks.failed = []() { std::fprintf(stderr, "[game] the connection failed\n"); };
    hooks.control = [this](const std::string& text) { onControl(text); };
    hooks.fast = [this](const uint8_t* data, size_t len) { onFast(data, len); };
    return hooks;
}

// onFast. Any thread: a beat is answered at once, and a rumble goes straight to the window.
void Game::Loop::onFast(const uint8_t* data, size_t len) {
    switch (data[0]) {
        case linkupwire::kTagRumble: {
            if (len < sizeof(linkupwire::Rumble)) break;
            linkupwire::Rumble r;
            std::memcpy(&r, data, sizeof r);
            Event e;
            e.kind = Event::Kind::Rumble;
            e.low = r.low;
            e.high = r.high;
            post(std::move(e));
            break;
        }
        case kTagBeat: {
            uint8_t back[9] = {};
            std::memcpy(back, data, std::min<size_t>(len, sizeof back));
            back[0] = kTagBeatBack;
            if (Rtc* r = live_.load()) r->sendFast(back, sizeof back);
            break;
        }
        case kTagBeatBack: {
            if (len < 9) break;
            int64_t sent = 0;
            std::memcpy(&sent, data + 1, sizeof sent);
            const int rtt = static_cast<int>(Rtc::nowMs() - sent);
            Rtc::onMain([this, rtt]() {
                if (session_) session_->rtt.push_back(rtt);
            });
            break;
        }
        default:
            break;
    }
}

// onControl.
void Game::Loop::onControl(const std::string& text) {
    Event e;
    if (text == "picture:1" || text == "picture:0") {
        e.kind = Event::Kind::Picture;
        e.on = text == "picture:1";
    } else if (text == "bye") {
        e.kind = Event::Kind::Gone;
    } else if (text.rfind("p:", 0) == 0) {
        e.kind = Event::Kind::Peer;
        e.text = text.substr(2);
    } else {
        return;
    }
    post(std::move(e));
}

// tick, the friend's part: the limit on connecting, the beat, the delay to the log, and a drop.
void Game::Loop::tick() {
    if (!session_) return;
    Session& s = *session_;
    const int64_t now = Rtc::nowMs();
    const auto failed = [&](const std::string& reason, const std::string& reported) {
        Event e;
        e.kind = Event::Kind::Failed;
        e.text = reason;
        post(std::move(e));
        if (s.server.report) s.server.report("failed", reported);
        s.startedMs = 0;
    };
    const bool limit = s.relayReason == "limit";
    if (!s.streamStarted) {
        // Nothing to wait for any longer, whichever part never came.
        if (s.startedMs != 0 && now - s.startedMs > kConnectLimitMs && !s.connected)
            failed(limit ? "relay_limit" : "other", "other");
        return;
    }
    Rtc* r = live_.load();
    if (!r) return;
    if (!s.connected && s.startedMs != 0 && now - s.startedMs > kConnectLimitMs)
        failed(limit ? "relay_limit" : "no_path", limit ? "relay_limit" : "no_path");
    if (now - s.lastBeatMs >= kBeatMs) {
        s.lastBeatMs = now;
        uint8_t beat[9];
        beat[0] = kTagBeat;
        std::memcpy(beat + 1, &now, sizeof now);
        r->sendFast(beat, sizeof beat);
    }
    // The delay goes to the log only, once a minute.
    if (now - s.lastRttLogMs >= 60000 && !s.rtt.empty()) {
        s.lastRttLogMs = now;
        std::sort(s.rtt.begin(), s.rtt.end());
        std::fprintf(stderr, "[game] %s, round trip median %d ms, worst %d ms\n", s.path.c_str(), s.rtt[s.rtt.size() / 2],
                     s.rtt.back());
        s.rtt.clear();
    }
    if (r->everHeard()) {
        const int64_t quiet = r->quietMs();
        if (!s.lost && quiet > kDropMs) {
            s.lost = true;
            std::fprintf(stderr, "[game] nothing heard from the host for %lld ms\n", static_cast<long long>(quiet));
            Event e;
            e.kind = Event::Kind::Lost;
            post(std::move(e));
        } else if (s.lost && quiet < 500) {
            s.lost = false;
            std::fprintf(stderr, "[game] the host is back\n");
            Event e;
            e.kind = Event::Kind::Back;
            post(std::move(e));
        }
    }
}

// endGame, and shutDown's goodbye.
void Game::Loop::stopSession(bool bye) {
    if (!session_) return;
    Rtc* r = live_.exchange(nullptr);
    if (bye && r) r->sendControl("bye");
    if (session_->stream) session_->stream->stop();
    session_.reset();
    pictures_.clear();
}

// ---- The window's side -----------------------------------------------------------

Game::Game(std::function<void()> wake) : loop_(std::make_unique<Loop>(std::move(wake))) {}
Game::~Game() = default;
bool Game::canPlay() { return loop_->canPlay(); }
void Game::begin(Server server) { loop_->begin(std::move(server)); }
void Game::ready(std::vector<std::string> stun, std::string turn, std::string relayReason) {
    loop_->ready(std::move(stun), std::move(turn), std::move(relayReason));
}
void Game::signal(int generation, const std::string& details) { loop_->signal(generation, details); }
void Game::pad(const linkupwire::Pad& pad) { loop_->pad(pad); }
void Game::tell(const std::string& word) { loop_->tell(word); }
void Game::end(bool bye) { loop_->end(bye); }
std::vector<Game::Event> Game::take() { return loop_->take(); }
bool Game::takePicture(const std::function<void(const Planes&)>& use) { return loop_->takePicture(use); }

}  // namespace game
