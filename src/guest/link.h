// The friend's side of a LinkUp server: what the console's LinkUp program does (conductor.cpp in
// CabinetOS) for a console that only ever joins. It pairs, signs in, keeps the list of friends,
// holds the one long `wait` request, opens an invite's sealed box for the game's name, and declines,
// joins, ends and leaves (PROTOCOL.md in cabinet-linkup).
//
// NOTHING HERE WAITS ON THE CALLER. Every request runs on one worker thread, in the order it was
// asked, and the long `wait` runs on a thread of its own and hands what it hears to the worker, so
// everything this keeps is the worker's alone. What happened comes back as events, which the window's
// thread takes each frame. `wake` is called after each one, from whichever thread made it, so the
// window can stop sleeping.
//
// It knows no window, no screen and no words: the app decides what each event shows.

#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <json-c/json.h>

#include "box.h"
#include "keys.h"
#include "linkuprules.h"
#include "noise.h"
#include "pairing.h"
#include "server_link.h"

namespace guest {

// A friend as the server lists one (PROTOCOL.md, "A friend").
struct Listed {
    std::string id;
    std::string username;     // as the server has it: what a box says
    std::string name;         // cleaned: what a person reads
    std::string server;       // cleaned
    std::string status;       // online, linkedup or offline
    int stream = 0;
    noise::Key key{};
    bool hasKey = false;
    std::string inviteId;
    std::string inviteSealed;
};

// The server's `friends` answer, read. Names are cleaned on the way in. Exposed for the test.
std::vector<Listed> readFriends(json_object* answer);

// The game an invite's box names, or empty when it does not open or does not fit: not sealed by that
// friend for this one, the wrong type, too old, or a count already seen. Exposed for the test.
std::string openInvite(const noise::KeyPair& mine, const std::string& myName, const Listed& from, box::Reader& reader,
                       int64_t now);

class Link {
public:
    // How the server answered lately, as the console's screen tells it.
    enum class Connection { Ok, Unreachable, Removed, AppBehind, ServerBehind };

    struct Event {
        enum class Kind {
            State,          // paired, server, connection
            Paired,         // pairing's result
            Friends,        // the list, fresh
            Invited,        // an invite arrived from `name`
            InviteEnded,    // `invite` ended, for `reason`
            Joined,         // the join's answer: `ok`, `session`
            SessionEnded,   // the server says `session` is over
            Left,           // this app has left its server
            Retyped,        // what answers at a new `address`: `answers`
        };
        Kind kind = Kind::State;
        bool paired = false;
        std::string server;              // the server's name
        std::string address;
        Connection connection = Connection::Ok;
        pairing::Result pairResult = pairing::Result::Failed;
        std::vector<linkup::Friend> friends;
        std::string name, invite, reason, session;
        bool ok = false;
        ServerLink::Place answers = ServerLink::Place::Nothing;
    };

    Link(std::string folder, std::function<void()> wake);
    ~Link();
    Link(const Link&) = delete;
    Link& operator=(const Link&) = delete;

    // The key, made the first time. False when the folder cannot hold it. Then the threads start.
    bool start();

    bool paired() const { return paired_.load(); }

    // Each is asked and returns at once; what happened comes back as an event.
    //
    // Pairing while paired is Change server's, with another server's code: pairing with it forgets
    // this one, as on the console.
    void pair(const std::string& address, const std::string& code, const std::string& username);
    // Change server, the address (cmdRetype): the key is what is checked, so the same server at a new
    // address is kept with no new pairing, and an address where nothing answers yet is kept too.
    // Another server's is not: it answers Another, and its code is asked for.
    void retype(const std::string& address);
    // `status` is online or offline.
    void signIn(const std::string& status);
    void setStatus(const std::string& status);
    // The server takes a sign-in under a new name, from the same account, as the name changing.
    void rename(const std::string& username);
    void decline(const std::string& invite);
    void join(const std::string& invite);
    void end(const std::string& session);
    void leave();

    // What has happened since last asked, oldest first.
    std::vector<Event> take();

    // Signs out, waiting at most `seconds`, and stops. For quitting.
    void stop(int seconds);

private:
    void work(std::function<void()> job);
    void workerLoop();
    void waitLoop();
    void post(Event e);

    // The worker's own.
    ServerLink::Outcome ask(const std::string& request, std::string& answer, json_object** out);
    void signInNow();
    void fetchFriends();
    void onEvents(const std::string& events);
    void setConnection(ServerLink::Outcome outcome);
    void sendState();

    std::string folder_;
    std::function<void()> wake_;
    Keys keys_;
    ServerLink link_;

    std::atomic<bool> stopping_{false};
    std::atomic<bool> paired_{false};
    std::atomic<int> generation_{0};     // a new server, or none: the long request starts over
    std::mutex workMu_;
    std::condition_variable workCv_;
    std::deque<std::function<void()>> jobs_;
    std::thread worker_, waiter_;

    std::mutex eventsMu_;
    std::vector<Event> events_;

    // The worker's alone.
    bool signedIn_ = false;
    std::string status_ = "online";
    Connection connection_ = Connection::Ok;
    std::vector<Listed> friends_;
    std::map<std::string, std::string> inviteGames_;    // invite id, the game its box named
    std::map<std::string, box::Reader> inviteReaders_;
    std::map<std::string, std::string> inviteSealed_;   // from the event, when a long list left it out
    std::set<std::string> saidInvited_;
};

}  // namespace guest
