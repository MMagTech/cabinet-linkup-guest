// 2pstart-cli: 2P Start on the command line, for trying its side of a LinkUp server by hand before
// there is a window. It speaks to the server exactly as a console does (PROTOCOL.md in
// MMagTech/cabinet-linkup), through the console's own code.
//
//   2pstart-cli [--folder <path>] pair <address> <code> <username>
//   2pstart-cli [--folder <path>] friends
//   2pstart-cli [--folder <path>] online      stays signed in and says what the server sends
//   2pstart-cli [--folder <path>] leave
//   2pstart-cli [--folder <path>] where       the folder it keeps its key in
//
// The address and the code are typed each time and kept nowhere but that folder.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include <curl/curl.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "box.h"
#include "guest/account.h"
#include "guest/paths.h"
#include "jsonutil.h"
#include "keys.h"
#include "pairing.h"
#include "server_link.h"

namespace {

std::atomic<bool> stopping{false};

void onInterrupt(int) { stopping.store(true); }

int usage() {
    std::fprintf(stderr,
                 "usage: 2pstart-cli [--folder <path>] pair <address> <code> <username>\n"
                 "       2pstart-cli [--folder <path>] friends | online | leave | where\n");
    return 2;
}

const char* wordsFor(pairing::Result r) {
    switch (r) {
        case pairing::Result::Paired: return "Paired.";
        case pairing::Result::WrongCode: return "Wrong or expired code.";
        case pairing::Result::TooManyTries: return "Too many tries. Wait a minute and try again.";
        case pairing::Result::Unreachable: return "No LinkUp server answered at that address.";
        case pairing::Result::NotThatServer: return "What answered there does not hold that code. Check the address.";
        case pairing::Result::Refused: return "The server refused.";
        case pairing::Result::Failed: return "Could not pair from this side.";
    }
    return "?";
}

const char* wordsFor(ServerLink::Outcome o) {
    switch (o) {
        case ServerLink::Outcome::Ok: return "Done.";
        case ServerLink::Outcome::Unreachable: return "Cannot reach the LinkUp server.";
        case ServerLink::Outcome::Removed: return "Removed from the LinkUp server.";
        case ServerLink::Outcome::ConsoleBehind: return "This app is too old for the server. It needs an update.";
        case ServerLink::Outcome::ServerBehind: return "The server is too old for this app. It needs an update.";
    }
    return "?";
}

// One operation. False, with the reason said, for anything but a clean answer.
bool ask(ServerLink& link, json_object* request, std::string& answer, long seconds = 15) {
    const std::string text = jsonutil::text(request);
    json_object_put(request);
    const ServerLink::Outcome outcome = link.call(text, answer, seconds);
    if (outcome != ServerLink::Outcome::Ok) {
        std::printf("%s\n", wordsFor(outcome));
        return false;
    }
    json_object* o = jsonutil::parse(answer);
    const std::string error = jsonutil::str(o, "error");
    if (o) json_object_put(o);
    if (!error.empty()) {
        std::printf("The server said: %s\n", error.c_str());
        return false;
    }
    return true;
}

json_object* requestNamed(const char* op) {
    json_object* q = json_object_new_object();
    jsonutil::set(q, "op", op);
    return q;
}

bool signIn(ServerLink& link, const std::string& folder) {
    const std::string account = guest::accountId(folder);
    const std::string name = guest::username(folder);
    if (account.empty() || name.empty()) {
        std::printf("Not paired yet. Run pair first.\n");
        return false;
    }
    json_object* q = requestNamed("signin");
    jsonutil::set(q, "account", account);
    jsonutil::set(q, "username", name);
    jsonutil::set(q, "status", "online");
    jsonutil::set(q, "hosts", false);
    std::string answer;
    return ask(link, q, answer);
}

bool sayFriends(ServerLink& link) {
    std::string answer;
    if (!ask(link, requestNamed("friends"), answer)) return false;
    json_object* o = jsonutil::parse(answer);
    json_object* list = jsonutil::get(o, "friends");
    const size_t count = list && json_object_is_type(list, json_type_array) ? json_object_array_length(list) : 0;
    if (count == 0) std::printf("No friends yet.\n");
    for (size_t i = 0; i < count; ++i) {
        json_object* f = json_object_array_get_idx(list, i);
        // Names came over the network, so they are cleaned before they reach a screen.
        const std::string name = box::cleanText(jsonutil::str(f, "username"), box::kMaxUsername);
        const std::string server = box::cleanText(jsonutil::str(f, "server"), box::kMaxUsername);
        std::printf("%s%s%s  %s%s\n", name.c_str(), server.empty() ? "" : " on ", server.c_str(),
                    jsonutil::str(f, "status").c_str(), jsonutil::get(f, "invite") ? "  (has invited you)" : "");
    }
    if (o) json_object_put(o);
    return true;
}

int pair(Keys& keys, const std::string& folder, const std::string& address, const std::string& code,
         const std::string& name) {
    if (!guest::usernameFits(name)) {
        std::printf("That name will not do: 1 to 128 bytes, with no control characters.\n");
        return 2;
    }
    if (const auto server = keys.server()) {
        std::printf("Already paired with %s. Run leave first.\n", server->name.c_str());
        return 1;
    }
    const std::string account = guest::accountFor(folder, code);
    if (account.empty()) {
        std::printf("%s\n", wordsFor(pairing::Result::WrongCode));
        return 1;
    }
    json_object* j = json_object_new_object();
    jsonutil::set(j, "kind", "console");
    jsonutil::set(j, "protocol", ServerLink::kProtocol);
    jsonutil::set(j, "oldest", ServerLink::kOldest);
    jsonutil::set(j, "stream", ServerLink::kStream);
    jsonutil::set(j, "account", account);
    jsonutil::set(j, "username", name);
    jsonutil::set(j, "hosts", false);  // 2P Start only joins (PROTOCOL.md, Who is joining)
    std::string serverName;
    const pairing::Result result = pairing::pair(keys, address, code, jsonutil::text(j), serverName);
    json_object_put(j);
    if (result != pairing::Result::Paired) {
        std::printf("%s\n", wordsFor(result));
        return 1;
    }
    if (!guest::keepAccount(folder, account) || !guest::keepUsername(folder, name)) {
        std::printf("Paired with %s, but could not keep it in %s\n", serverName.c_str(), folder.c_str());
        return 2;
    }
    std::printf("Paired with %s as %s.\n", serverName.c_str(), name.c_str());
    return 0;
}

int friends(Keys& keys, const std::string& folder) {
    ServerLink link(keys);
    if (!signIn(link, folder)) return 1;
    const bool ok = sayFriends(link);
    std::string answer;
    ask(link, requestNamed("signout"), answer);
    return ok ? 0 : 1;
}

// The one long request, as a console holds it (PROTOCOL.md, `wait`), on a thread of its own so that
// Ctrl-C is heard at once and can sign out.
void waitLoop(ServerLink& link, const std::string& folder) {
    long after = 0;
    std::string epoch;
    while (!stopping.load()) {
        std::string answer;
        const ServerLink::Outcome outcome =
            link.call("{\"op\":\"wait\",\"after\":" + std::to_string(after) + "}", answer, 40);
        if (stopping.load()) return;
        if (outcome != ServerLink::Outcome::Ok) {
            std::printf("%s\n", wordsFor(outcome));
            if (outcome == ServerLink::Outcome::Removed) {
                stopping.store(true);
                return;
            }
            link.waitBeforeRetry([] { return stopping.load(); });
            continue;
        }
        json_object* o = jsonutil::parse(answer);
        if (!o) continue;
        const std::string nowEpoch = jsonutil::str(o, "epoch");
        // The server restarted: sign in again, and start over at 0.
        const bool restarted = !epoch.empty() && epoch != nowEpoch;
        epoch = nowEpoch;
        after = restarted ? 0 : static_cast<long>(jsonutil::num(o, "next"));
        if (restarted) {
            std::printf("The server restarted.\n");
            signIn(link, folder);
        }
        json_object* events = jsonutil::get(o, "events");
        const size_t count =
            !restarted && events && json_object_is_type(events, json_type_array) ? json_object_array_length(events) : 0;
        for (size_t i = 0; i < count; ++i) {
            json_object* e = json_object_array_get_idx(events, i);
            const std::string type = jsonutil::str(e, "type");
            if (type == "friends") {
                std::printf("Friends changed:\n");
                sayFriends(link);
            } else if (type == "invite") {
                std::printf("An invite arrived.\n");
                sayFriends(link);
            } else if (type == "invite_ended") {
                std::printf("The invite ended: %s\n", jsonutil::str(e, "reason").c_str());
            } else if (type == "removed") {
                std::printf("%s\n", wordsFor(ServerLink::Outcome::Removed));
                stopping.store(true);
            } else {
                std::printf("The server sent: %s\n", type.c_str());
            }
        }
        std::fflush(stdout);
        json_object_put(o);
    }
}

int online(Keys& keys, const std::string& folder) {
    static ServerLink link(keys);   // outlives the waiting thread, which is left behind at exit
    if (!signIn(link, folder)) return 1;
    std::printf("Online as %s. Ctrl-C to stop.\n", guest::username(folder).c_str());
    sayFriends(link);
    std::fflush(stdout);
    std::signal(SIGINT, onInterrupt);
    static const std::string kept = folder;
    std::thread([] { waitLoop(link, kept); }).detach();
    while (!stopping.load()) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string answer;
    link.call("{\"op\":\"signout\"}", answer, 5);
    std::printf("Offline.\n");
    std::fflush(stdout);
    // The long request may still be held by the server. Nothing is left to save, so leave now.
    std::_Exit(0);
}

int leave(Keys& keys) {
    const auto server = keys.server();
    if (!server) {
        std::printf("Not paired.\n");
        return 0;
    }
    ServerLink link(keys);
    std::string answer;
    const ServerLink::Outcome outcome = link.call("{\"op\":\"leave\"}", answer);
    if (outcome != ServerLink::Outcome::Ok && outcome != ServerLink::Outcome::Removed) {
        std::printf("%s Nothing changed.\n", wordsFor(outcome));
        return 1;
    }
    keys.forget();
    std::printf("Left %s.\n", server->name.c_str());
    return 0;
}

int run(std::vector<std::string> args) {
    std::string folder = guest::dataFolder();
    if (args.size() >= 2 && args[0] == "--folder") {
        folder = args[1];
        args.erase(args.begin(), args.begin() + 2);
    }
    if (args.empty()) return usage();
    const std::string action = args[0];
    if (folder.empty()) {
        std::fprintf(stderr, "This computer names no folder to keep a key in. Give one with --folder.\n");
        return 2;
    }
    if (action == "where" && args.size() == 1) {
        std::printf("%s\n", folder.c_str());
        return 0;
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);
    Keys keys(folder);
    if (!keys.load()) {
        std::fprintf(stderr, "Cannot keep a key in %s\n", folder.c_str());
        return 2;
    }
    if (action == "pair" && args.size() == 4) return pair(keys, folder, args[1], args[2], args[3]);
    if (action == "friends" && args.size() == 1) return friends(keys, folder);
    if (action == "online" && args.size() == 1) return online(keys, folder);
    if (action == "leave" && args.size() == 1) return leave(keys);
    return usage();
}

}  // namespace

#ifdef _WIN32
// Windows hands a program its words in UTF-16. Everything inside is UTF-8.
int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        const int n = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
        std::string arg(static_cast<size_t>(n > 0 ? n : 1), '\0');
        if (n > 0) WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, arg.data(), n, nullptr, nullptr);
        arg.resize(arg.size() - 1);
        args.push_back(arg);
    }
    return run(args);
}
#else
int main(int argc, char** argv) { return run(std::vector<std::string>(argv + 1, argv + argc)); }
#endif
