// The friend's side of a LinkUp server (link.h). Each part names the function of the console's
// conductor.cpp it follows, so the two can be read side by side.

#include "guest/link.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <memory>

#include "guest/account.h"
#include "hexutil.h"
#include "jsonutil.h"

namespace guest {

namespace {

using jsonutil::get;
using jsonutil::str;

json_object* requestNamed(const char* op) {
    json_object* o = json_object_new_object();
    jsonutil::set(o, "op", op);
    return o;
}

std::string textOf(json_object* request) {
    const std::string text = jsonutil::text(request);
    json_object_put(request);
    return text;
}

constexpr int64_t kBoxLifeSeconds = 300;

// A user name or a password inside a URL: every byte but the unreserved ones written as %XX, as
// GLib's g_uri_escape_string does for the console.
std::string escaped(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char ch : s) {
        if (std::isalnum(ch) || ch == '-' || ch == '.' || ch == '_' || ch == '~') {
            out += static_cast<char>(ch);
        } else {
            out += '%';
            out += hex[ch >> 4];
            out += hex[ch & 15];
        }
    }
    return out;
}

linkup::Status statusOf(const Listed& f, bool invited) {
    if (invited) return linkup::Status::InvitedYou;
    if (f.status == "online") return linkup::Status::Online;
    if (f.status == "linkedup") return linkup::Status::InGame;
    return linkup::Status::Offline;
}

}  // namespace

// ---- What the test reaches --------------------------------------------------

// fetchFriends, its reading part.
std::vector<Listed> readFriends(json_object* answer) {
    std::vector<Listed> out;
    json_object* list = get(answer, "friends");
    if (!list || !json_object_is_type(list, json_type_array)) return out;
    for (size_t i = 0; i < json_object_array_length(list); ++i) {
        json_object* j = json_object_array_get_idx(list, i);
        Listed f;
        f.id = str(j, "id");
        f.username = str(j, "username");
        f.name = box::cleanText(f.username, box::kMaxUsername);
        f.server = box::cleanText(str(j, "server"), box::kMaxUsername);
        f.status = str(j, "status");
        f.stream = static_cast<int>(jsonutil::num(j, "stream"));
        std::vector<uint8_t> key;
        if (hexutil::fromBase64(str(j, "key"), key) && key.size() == f.key.size()) {
            std::copy(key.begin(), key.end(), f.key.begin());
            f.hasKey = true;
        }
        if (json_object* invite = get(j, "invite")) {
            f.inviteId = str(invite, "id");
            f.inviteSealed = str(invite, "sealed");
        }
        if (f.id.empty() || f.name.empty()) continue;
        out.push_back(std::move(f));
    }
    return out;
}

// openInviteBox.
std::string openInvite(const noise::KeyPair& mine, const std::string& myName, const Listed& from, box::Reader& reader,
                       int64_t now) {
    std::vector<uint8_t> raw;
    if (!from.hasKey || from.inviteId.empty() || !hexutil::fromBase64(from.inviteSealed, raw)) return {};
    noise::Bytes plain;
    if (!box::open(mine, from.key, from.inviteId, raw, plain)) return {};
    box::Contents c;
    if (!reader.read(plain, "invite", from.username, myName, now, c)) return {};
    return c.game;
}

// turnUri, conductor.cpp.
std::string turnUri(const std::string& url, const std::string& user, const std::string& password) {
    if (url.rfind("turn:", 0) != 0) return {};
    std::string hostPort = url.substr(5);
    const size_t q = hostPort.find('?');
    if (q != std::string::npos) hostPort = hostPort.substr(0, q);
    return "turn://" + escaped(user) + ":" + escaped(password) + "@" + hostPort;
}

// ---- Asked by the window --------------------------------------------------

Link::Link(std::string folder, std::function<void()> wake)
    : folder_(std::move(folder)), wake_(std::move(wake)), keys_(folder_), link_(keys_) {}

Link::~Link() { stop(0); }

bool Link::start() {
    if (!keys_.load()) return false;
    paired_ = keys_.server().has_value();
    worker_ = std::thread([this]() { workerLoop(); });
    waiter_ = std::thread([this]() { waitLoop(); });
    // The last run ended in a session: the server has to hear it is over (Conductor::start).
    const std::string left = keys_.openSession();
    keys_.setOpenSession("");
    if (!left.empty() && paired_.load()) end(left);
    work([this]() { sendState(); });
    return true;
}

// cmdPair.
void Link::pair(const std::string& address, const std::string& code, const std::string& username) {
    work([this, address, code, username]() {
        Event e;
        e.kind = Event::Kind::Paired;
        const std::string account = accountId(folder_);
        if (account.empty() || !usernameFits(username)) {
            e.pairResult = pairing::Result::Failed;
            post(std::move(e));
            return;
        }
        json_object* j = json_object_new_object();
        jsonutil::set(j, "kind", "console");
        jsonutil::set(j, "protocol", ServerLink::kProtocol);
        jsonutil::set(j, "oldest", ServerLink::kOldest);
        jsonutil::set(j, "stream", ServerLink::kStream);
        jsonutil::set(j, "account", account);
        jsonutil::set(j, "username", username);
        jsonutil::set(j, "hosts", false);  // 2P Start only joins (PROTOCOL.md, Who is joining)
        std::string serverName;
        e.pairResult = pairing::pair(keys_, address, code, textOf(j), serverName);
        e.server = serverName;
        if (e.pairResult == pairing::Result::Paired) {
            keepUsername(folder_, username);
            link_.reset();
            paired_ = true;
            connection_ = Connection::Ok;
            ++generation_;
            // Another server's friends and invites are not this one's.
            friends_.clear();
            inviteGames_.clear();
            inviteReaders_.clear();
            inviteSealed_.clear();
            game_.reset();
        }
        const bool again = e.pairResult == pairing::Result::Paired && signedIn_;
        post(std::move(e));
        sendState();
        if (again) signInNow();
    });
}

// cmdRetype.
void Link::retype(const std::string& address) {
    work([this, address]() {
        Event e;
        e.kind = Event::Kind::Retyped;
        e.address = address;
        e.answers = link_.whatAnswers(address);
        if (e.answers == ServerLink::Place::Another) {
            post(std::move(e));
            return;
        }
        keys_.setAddress(address);
        link_.reset();
        ++generation_;
        post(std::move(e));
        std::string answer;
        const ServerLink::Outcome outcome = link_.call("{\"op\":\"config\"}", answer);
        // A new address is a new place to be signed in at. Coming back from out of reach signs in
        // by itself (setConnection); staying reachable does not, so it is done here.
        const bool wasOk = connection_ == Connection::Ok;
        setConnection(outcome);
        sendState();
        if (outcome == ServerLink::Outcome::Ok && wasOk && signedIn_) signInNow();
    });
}

// cmdSignin.
void Link::signIn(const std::string& status) {
    work([this, status]() {
        status_ = status;
        signedIn_ = true;
        signInNow();
    });
}

void Link::setStatus(const std::string& status) {
    work([this, status]() {
        status_ = status;
        if (!signedIn_) return;
        json_object* q = requestNamed("status");
        jsonutil::set(q, "status", status);
        std::string answer;
        ask(textOf(q), answer, nullptr);
    });
}

void Link::rename(const std::string& username) {
    work([this, username]() {
        if (!keepUsername(folder_, username)) return;
        if (signedIn_) signInNow();
    });
}

void Link::decline(const std::string& invite) {
    work([this, invite]() {
        json_object* q = requestNamed("decline");
        jsonutil::set(q, "invite", invite);
        std::string answer;
        ask(textOf(q), answer, nullptr);
    });
}

// cmdJoin. The host's name and key are taken before asking: a sign-in on the way can fetch the list
// afresh.
void Link::join(const std::string& invite, bool play, bool h265) {
    work([this, invite, play, h265]() {
        Event e;
        e.kind = Event::Kind::Joined;
        e.invite = invite;
        auto it = std::find_if(friends_.begin(), friends_.end(), [&](const Listed& f) { return f.inviteId == invite; });
        if (it == friends_.end() || !it->hasKey) {
            post(std::move(e));
            return;
        }
        Game g;
        g.invite = invite;
        g.host = it->username;
        g.hostKey = it->key;
        json_object* q = requestNamed("join");
        jsonutil::set(q, "invite", invite);
        std::string answer;
        json_object* o = nullptr;
        const ServerLink::Outcome outcome = ask(textOf(q), answer, &o);
        e.session = str(o, "session");
        e.ok = outcome == ServerLink::Outcome::Ok && !e.session.empty();
        if (o) json_object_put(o);
        // Kept until it ends, so a run that dies in it can end it next time.
        if (e.ok) keys_.setOpenSession(e.session);
        const bool playing = e.ok && play;
        if (playing) {
            g.session = e.session;
            game_ = std::move(g);
        }
        post(std::move(e));
        if (!playing) return;
        // sendHello: what this app can decode, and the stream version it speaks. H.264 always; H.265
        // too where the graphics chip decodes it, and then the host sends that, as to a console.
        json_object* hello = json_object_new_object();
        jsonutil::set(hello, "stream", ServerLink::kStream);
        jsonutil::set(hello, "oldest", ServerLink::kStream);
        jsonutil::set(hello, "h265", h265);
        jsonutil::set(hello, "h264", true);
        json_object* wrap = json_object_new_object();
        json_object_object_add(wrap, "hello", hello);
        sendSignal(1, textOf(wrap));
        prepareConnection();
    });
}

void Link::signal(int generation, const std::string& details) {
    work([this, generation, details]() { sendSignal(generation, details); });
}

void Link::report(const std::string& path, const std::string& reason) {
    work([this, path, reason]() {
        if (!game_) return;
        json_object* q = requestNamed("report");
        jsonutil::set(q, "session", game_->session);
        jsonutil::set(q, "path", path);
        jsonutil::set(q, "reason", reason);
        std::string answer;
        ask(textOf(q), answer, nullptr);
    });
}

// cmdEnd.
void Link::end(const std::string& session) {
    work([this, session]() {
        json_object* q = requestNamed("end");
        jsonutil::set(q, "session", session);
        std::string answer;
        ask(textOf(q), answer, nullptr);
        if (keys_.openSession() == session) keys_.setOpenSession("");
        if (game_ && game_->session == session) game_.reset();
    });
}

// cmdLeave. Off the server it goes either way: a removed app is let go too.
void Link::leave() {
    work([this]() {
        std::string answer;
        link_.call(textOf(requestNamed("leave")), answer);
        keys_.forget();
        link_.reset();
        paired_ = false;
        signedIn_ = false;
        friends_.clear();
        game_.reset();
        connection_ = Connection::Ok;
        ++generation_;
        Event e;
        e.kind = Event::Kind::Left;
        post(std::move(e));
        sendState();
    });
}

std::vector<Link::Event> Link::take() {
    std::lock_guard<std::mutex> lk(eventsMu_);
    std::vector<Event> out;
    out.swap(events_);
    return out;
}

// The long request may still be held by the server, so the waiting thread is let go rather than
// joined. It holds nothing worth saving.
//
// A worker still busy past `seconds` is let go too: the program is ending, and does so at once.
void Link::stop(int seconds) {
    if (!worker_.joinable()) return;
    auto done = std::make_shared<std::atomic<bool>>(false);
    if (seconds > 0) {
        work([this, done]() {
            if (signedIn_) {
                std::string answer;
                link_.call(textOf(requestNamed("signout")), answer, 5);
            }
            done->store(true);
        });
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        while (!done->load() && std::chrono::steady_clock::now() < until)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    stopping_ = true;
    workCv_.notify_all();
    if (done->load() || seconds == 0) worker_.join();
    else worker_.detach();
    if (waiter_.joinable()) waiter_.detach();
}

// ---- The worker ------------------------------------------------------------

void Link::work(std::function<void()> job) {
    std::lock_guard<std::mutex> lk(workMu_);
    jobs_.push_back(std::move(job));
    workCv_.notify_one();
}

void Link::workerLoop() {
    while (!stopping_.load()) {
        std::function<void()> next;
        {
            std::unique_lock<std::mutex> lk(workMu_);
            workCv_.wait(lk, [this]() { return !jobs_.empty() || stopping_.load(); });
            if (jobs_.empty()) continue;
            next = std::move(jobs_.front());
            jobs_.pop_front();
        }
        next();
    }
}

void Link::post(Event e) {
    {
        std::lock_guard<std::mutex> lk(eventsMu_);
        events_.push_back(std::move(e));
    }
    if (wake_) wake_();
}

// send: one request, and the server's answer. A request refused because the server has forgotten
// who is signed in here is asked once more after signing in again, so a press made just after a
// restart still works.
ServerLink::Outcome Link::ask(const std::string& request, std::string& answer, json_object** out) {
    ServerLink::Outcome outcome = link_.call(request, answer);
    setConnection(outcome);
    json_object* o = jsonutil::parse(answer);
    if (outcome == ServerLink::Outcome::Ok && signedIn_ && str(o, "error") == "no_account") {
        if (o) json_object_put(o);
        signInNow();
        outcome = link_.call(request, answer);
        setConnection(outcome);
        o = jsonutil::parse(answer);
    }
    if (out) *out = o;
    else if (o) json_object_put(o);
    return outcome;
}

// signIn: the account and name kept at pairing, and the status the window last gave. Then the list.
void Link::signInNow() {
    const std::string account = accountId(folder_);
    const std::string name = username(folder_);
    if (account.empty() || name.empty() || !paired_.load()) return;
    json_object* q = requestNamed("signin");
    jsonutil::set(q, "account", account);
    jsonutil::set(q, "username", name);
    jsonutil::set(q, "status", status_);
    jsonutil::set(q, "hosts", false);
    std::string answer;
    const ServerLink::Outcome outcome = link_.call(textOf(q), answer);
    setConnection(outcome);
    json_object* o = jsonutil::parse(answer);
    const bool ok = outcome == ServerLink::Outcome::Ok && str(o, "error").empty();
    if (o) json_object_put(o);
    if (ok) fetchFriends();
}

// fetchFriends and sendFriends: the list, each invite's box opened once, and a new invite said once.
void Link::fetchFriends() {
    if (!signedIn_) return;
    std::string answer;
    json_object* o = nullptr;
    const ServerLink::Outcome outcome = ask(textOf(requestNamed("friends")), answer, &o);
    if (outcome != ServerLink::Outcome::Ok || !o || !str(o, "error").empty()) {
        if (o) json_object_put(o);
        return;
    }
    friends_ = readFriends(o);
    json_object_put(o);

    const std::string me = username(folder_);
    Event list;
    list.kind = Event::Kind::Friends;
    std::vector<Event> arrived;
    for (Listed& f : friends_) {
        std::string game;
        if (!f.inviteId.empty()) {
            if (f.inviteSealed.empty()) {
                auto held = inviteSealed_.find(f.inviteId);
                if (held != inviteSealed_.end()) f.inviteSealed = held->second;
            }
            // A box only needs opening once, and its reader would refuse its count a second time.
            auto known = inviteGames_.find(f.inviteId);
            if (known != inviteGames_.end()) {
                game = known->second;
            } else {
                game = openInvite(keys_.mine(), me, f, inviteReaders_[f.inviteId], std::time(nullptr));
                inviteGames_[f.inviteId] = game;
            }
        }
        const bool invited = !game.empty();
        linkup::Friend shown(f.name, f.server, statusOf(f, invited), invited ? game : "");
        shown.id = f.id;
        shown.invite = invited ? f.inviteId : "";
        shown.stream = f.stream;
        list.friends.push_back(std::move(shown));
        if (invited && saidInvited_.insert(f.inviteId).second) {
            Event e;
            e.kind = Event::Kind::Invited;
            e.name = f.name;
            e.invite = f.inviteId;
            arrived.push_back(std::move(e));
        }
    }
    post(std::move(list));
    for (Event& e : arrived) post(std::move(e));
}

// onEvents.
void Link::onEvents(const std::string& text) {
    json_object* events = jsonutil::parse(text);
    if (!events || !json_object_is_type(events, json_type_array)) {
        if (events) json_object_put(events);
        return;
    }
    bool refresh = false;
    for (size_t i = 0; i < json_object_array_length(events); ++i) {
        json_object* ev = json_object_array_get_idx(events, i);
        const std::string type = str(ev, "type");
        if (type == "friends") {
            refresh = true;
        } else if (type == "invite") {
            inviteSealed_[str(ev, "invite")] = str(ev, "sealed");
            refresh = true;
        } else if (type == "invite_ended") {
            Event e;
            e.kind = Event::Kind::InviteEnded;
            e.invite = str(ev, "invite");
            e.reason = str(ev, "reason");
            post(std::move(e));
            refresh = true;
        } else if (type == "signal") {
            if (game_ && str(ev, "session") == game_->session) openSignal(str(ev, "sealed"));
        } else if (type == "session_ended") {
            Event e;
            e.kind = Event::Kind::SessionEnded;
            e.session = str(ev, "session");
            if (keys_.openSession() == e.session) keys_.setOpenSession("");
            if (game_ && game_->session == e.session) game_.reset();
            post(std::move(e));
        } else if (type == "removed") {
            setConnection(ServerLink::Outcome::Removed);
        }
    }
    json_object_put(events);
    if (refresh) fetchFriends();
}

// setConnection: a change is said, and the server coming back is signed in to again.
void Link::setConnection(ServerLink::Outcome outcome) {
    Connection now = Connection::Ok;
    switch (outcome) {
        case ServerLink::Outcome::Ok: now = Connection::Ok; break;
        case ServerLink::Outcome::Unreachable: now = Connection::Unreachable; break;
        case ServerLink::Outcome::Removed: now = Connection::Removed; break;
        case ServerLink::Outcome::ConsoleBehind: now = Connection::AppBehind; break;
        case ServerLink::Outcome::ServerBehind: now = Connection::ServerBehind; break;
    }
    if (now == connection_) return;
    connection_ = now;
    sendState();
    if (now == Connection::Ok && signedIn_) signInNow();
}

void Link::sendState() {
    const auto server = keys_.server();
    Event e;
    e.kind = Event::Kind::State;
    e.paired = server.has_value();
    e.server = server ? server->name : std::string();
    e.address = server ? server->address : std::string();
    e.connection = connection_;
    post(std::move(e));
}

// ---- A game ------------------------------------------------------------------

// sendSignal: the details in a box only the host can open, sealed to this invite and counted.
void Link::sendSignal(int generation, const std::string& details) {
    if (!game_) return;
    json_object* wrap = json_object_new_object();
    jsonutil::set(wrap, "g", generation);
    jsonutil::set(wrap, "d", details);
    box::Contents c;
    c.type = "signal";
    c.n = game_->counter.take();
    c.expires = static_cast<int64_t>(std::time(nullptr)) + kBoxLifeSeconds;
    c.from = username(folder_);
    c.to = game_->host;
    c.details = textOf(wrap);
    const std::string text = box::write(c);
    noise::Bytes sealed;
    if (!box::seal(keys_.mine(), game_->hostKey, game_->invite, noise::Bytes(text.begin(), text.end()), sealed)) return;
    json_object* q = requestNamed("signal");
    jsonutil::set(q, "session", game_->session);
    jsonutil::set(q, "sealed", hexutil::toBase64(sealed));
    std::string answer;
    ask(textOf(q), answer, nullptr);
}

// onSignalBox, its opening: a box the host sealed for this invite, read once, and what it carries
// handed on with its connection's number. Anything that does not open or fit is dropped.
void Link::openSignal(const std::string& sealed) {
    std::vector<uint8_t> raw;
    if (!hexutil::fromBase64(sealed, raw)) return;
    noise::Bytes plain;
    if (!box::open(keys_.mine(), game_->hostKey, game_->invite, raw, plain)) return;
    box::Contents c;
    if (!game_->reader.read(plain, "signal", game_->host, username(folder_), std::time(nullptr), c)) return;
    json_object* outer = jsonutil::parse(c.details);
    if (!outer) return;
    Event e;
    e.kind = Event::Kind::Signal;
    e.generation = static_cast<int>(jsonutil::num(outer, "g", 1));
    e.details = str(outer, "d");
    json_object_put(outer);
    if (!e.details.empty()) post(std::move(e));
}

// prepareConnection, the server's half: its STUN list and the relay. Which STUN server answers is
// the game's to find out, off this thread.
void Link::prepareConnection() {
    const std::string session = game_->session;
    Event e;
    e.kind = Event::Kind::Ready;
    std::string answer;
    json_object* o = nullptr;
    ask("{\"op\":\"config\"}", answer, &o);
    if (json_object* stun = get(o, "stun"); stun && json_object_is_type(stun, json_type_array)) {
        for (size_t i = 0; i < json_object_array_length(stun); ++i) {
            json_object* one = json_object_array_get_idx(stun, i);
            if (json_object_is_type(one, json_type_string)) e.stun.push_back(json_object_get_string(one));
        }
    }
    if (o) json_object_put(o);
    o = nullptr;
    json_object* q = requestNamed("relay");
    jsonutil::set(q, "session", session);
    ask(textOf(q), answer, &o);
    json_object* relay = get(o, "relay");
    if (relay && json_object_is_type(relay, json_type_object)) {
        json_object* urls = get(relay, "urls");
        if (urls && json_object_is_type(urls, json_type_array) && json_object_array_length(urls) > 0) {
            json_object* first = json_object_array_get_idx(urls, 0);
            if (json_object_is_type(first, json_type_string))
                e.turn = turnUri(json_object_get_string(first), str(relay, "username"), str(relay, "password"));
        }
    } else {
        e.reason = str(o, "reason");
    }
    if (o) json_object_put(o);
    if (game_ && game_->session == session) post(std::move(e));
}

// ---- The long request ------------------------------------------------------

// waitLoop. It only asks and listens: what it hears is the worker's to act on.
void Link::waitLoop() {
    long after = 0;
    std::string epoch;
    int generation = generation_.load();
    bool missed = false;
    const auto stop = [this, &generation]() { return stopping_.load() || generation != generation_.load(); };
    while (!stopping_.load()) {
        if (generation != generation_.load()) {
            generation = generation_.load();
            after = 0;
            epoch.clear();
        }
        if (!paired_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }
        // After a miss, a short request first: the long one is held up to 25 seconds, so the server
        // would be seen back only that much later.
        if (missed) {
            std::string answer;
            const ServerLink::Outcome outcome = link_.call("{\"op\":\"config\"}", answer);
            if (outcome == ServerLink::Outcome::Unreachable) {
                link_.waitBeforeRetry(stop);
                continue;
            }
            missed = false;
            work([this, outcome]() { setConnection(outcome); });
        }
        std::string answer;
        const ServerLink::Outcome outcome =
            link_.call("{\"op\":\"wait\",\"after\":" + std::to_string(after) + "}", answer, 40);
        if (stopping_.load()) return;
        if (outcome != ServerLink::Outcome::Ok) {
            missed = outcome == ServerLink::Outcome::Unreachable;
            work([this, outcome]() { setConnection(outcome); });
            link_.waitBeforeRetry(stop);
            continue;
        }
        json_object* o = jsonutil::parse(answer);
        if (!o) continue;
        const std::string nowEpoch = str(o, "epoch");
        // The server restarted, perhaps too quickly to be seen as gone: sign in again, from 0.
        const bool restarted = !epoch.empty() && epoch != nowEpoch;
        epoch = nowEpoch;
        after = restarted ? 0 : static_cast<long>(jsonutil::num(o, "next"));
        std::string events = "[]";
        if (json_object* list = get(o, "events")) events = jsonutil::text(list);
        json_object_put(o);
        work([this, events, restarted]() {
            setConnection(ServerLink::Outcome::Ok);
            if (restarted) {
                if (signedIn_) signInNow();
                return;
            }
            onEvents(events);
        });
    }
}

}  // namespace guest
