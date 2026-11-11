// The app's parts that need no window and no network: where focus goes, what a controller, the
// keys and the mouse ask for, what a typed line keeps, the settings file, the server's list of
// friends read with each invite's sealed box opened, the idle rule, and Startup's entry.
//
//   app-test

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

#include "app/idle.h"
#include "app/input.h"
#include "app/moves.h"
#include "app/startup.h"
#include "app/textline.h"
#include "app/tray_system.h"
#include "box.h"
#include "fileio.h"
#include "guest/link.h"
#include "guest/settings.h"
#include "hexutil.h"
#include "jsonutil.h"

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (ok) return;
    ++failures;
    std::fprintf(stderr, "FAILED: %s\n", what.c_str());
}

using moves::Move;
using Then = moves::Step::Then;

bool is(const moves::Step& s, int focus, Then then) { return s.focus == focus && s.then == then; }

void focusMovesAsOnTheConsole() {
    // Ten friends: rows of four, four and two.
    check(is(moves::friends(0, 10, Move::Up), -1, Then::Moved), "up from the first row reaches the Status line");
    check(is(moves::friends(-1, 10, Move::Up), -1, Then::ToBar), "up from the Status line reaches the bar");
    check(is(moves::friends(-1, 10, Move::Down), 0, Then::Moved), "down from the Status line reaches the first tile");
    check(is(moves::friends(-1, 10, Move::Press), -1, Then::Press), "the Status line is pressed");
    check(is(moves::friends(-1, 10, Move::Left), -1, Then::Edge), "the Status line goes nowhere sideways");
    check(is(moves::friends(1, 10, Move::Down), 5, Then::Moved), "down goes a row");
    check(is(moves::friends(7, 10, Move::Down), 9, Then::Moved), "down onto a short row lands on its last tile");
    check(is(moves::friends(9, 10, Move::Down), 9, Then::Edge), "the last row goes no further down");
    check(is(moves::friends(5, 10, Move::Up), 1, Then::Moved), "up goes a row");
    check(is(moves::friends(4, 10, Move::Left), 4, Then::Edge), "a row's first tile goes no further left");
    check(is(moves::friends(3, 10, Move::Right), 3, Then::Edge), "a row's last tile does not wrap");
    check(is(moves::friends(9, 10, Move::Right), 9, Then::Edge), "the last tile goes no further right");
    check(is(moves::friends(2, 10, Move::Press), 2, Then::Press), "a tile is pressed");
    check(is(moves::friends(2, 10, Move::Back), 2, Then::Back), "Back is Back");
    check(is(moves::friends(-1, 0, Move::Up), -1, Then::ToBar), "an empty list leaves only the bar");
    check(is(moves::friends(-1, 0, Move::Down), -1, Then::Edge), "an empty list has nothing to go down to");

    check(is(moves::column(0, 7, Move::Up, true), 0, Then::ToBar), "up from Settings' first row reaches the bar");
    check(is(moves::column(0, 4, Move::Up, false), 0, Then::Edge), "first run has no bar to go up to");
    check(is(moves::column(2, 4, Move::Down, false), 3, Then::Moved), "down reaches Connect");
    check(is(moves::column(3, 4, Move::Down, false), 3, Then::Edge), "Connect is the last");
    check(is(moves::column(1, 2, Move::Press, false), 1, Then::Press), "a panel's answer is pressed");

    moves::BarStep b = moves::bar(0, 2, Move::Right, true);
    check(b.at == 1 && !b.leave && !b.edge, "right on the bar switches to Settings");
    b = moves::bar(1, 2, Move::Right, true);
    check(b.at == 1 && b.edge, "the bar does not wrap");
    b = moves::bar(1, 2, Move::PrevScreen, true);
    check(b.at == 0, "a shoulder switches the screen");
    b = moves::bar(0, 2, Move::Down, true);
    check(b.leave, "down leaves the bar for the screen");
    b = moves::bar(0, 2, Move::Back, false);
    check(!b.leave && b.edge, "nothing to focus below keeps focus on the bar");

    check(moves::topFor(0, 0, 10, 6) == 0, "the top of a list shows its top");
    check(moves::topFor(6, 0, 10, 6) == 1, "a list scrolls one to show the next");
    check(moves::topFor(9, 0, 10, 6) == 4, "the end of a list shows its end");
    check(moves::topFor(2, 4, 10, 6) == 2, "a list scrolls back up to what is focused");
    check(moves::topFor(3, 0, 4, 6) == 0, "a short list does not scroll");
    std::printf("  focus moves as on the console\n");
}

SDL_Event key(SDL_EventType type, SDL_Keycode k, bool repeat = false, SDL_Keymod mod = SDL_KMOD_NONE) {
    SDL_Event e{};
    e.type = type;
    e.key.key = k;
    e.key.repeat = repeat;
    e.key.mod = mod;
    return e;
}

SDL_Event pad(SDL_EventType type, int button, SDL_JoystickID which = 1) {
    SDL_Event e{};
    e.type = type;
    e.gbutton.which = which;
    e.gbutton.button = static_cast<Uint8>(button);
    return e;
}

SDL_Event axis(int a, int value, SDL_JoystickID which = 1) {
    SDL_Event e{};
    e.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    e.gaxis.which = which;
    e.gaxis.axis = static_cast<Uint8>(a);
    e.gaxis.value = static_cast<Sint16>(value);
    return e;
}

void handsBecomeMoves() {
    input::Translator t;
    std::optional<parts::Hands> hands;
    check(t.take(key(SDL_EVENT_KEY_DOWN, SDLK_RETURN), hands) == Move::Press && hands == parts::Hands::Keyboard,
          "Enter presses, and the keys were touched");
    check(t.take(key(SDL_EVENT_KEY_DOWN, SDLK_ESCAPE), hands) == Move::Back, "Esc is Back");
    check(t.take(key(SDL_EVENT_KEY_DOWN, SDLK_TAB, false, SDL_KMOD_LSHIFT), hands) == Move::Up, "Shift-Tab goes up");
    check(!t.take(key(SDL_EVENT_KEY_DOWN, SDLK_V, false, SDL_KMOD_LGUI), hands), "Command-V is not a move");
    check(!t.take(key(SDL_EVENT_KEY_DOWN, SDLK_A), hands), "a letter is not a move");

    hands.reset();
    check(t.take(pad(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_SOUTH), hands) == Move::Press &&
              hands == parts::Hands::Controller,
          "A presses, and the controller was touched");
    check(t.take(pad(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_EAST), hands) == Move::Back, "B is Back");
    check(t.take(pad(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER), hands) == Move::NextScreen,
          "a shoulder changes screen");
    check(!t.take(pad(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_GUIDE), hands), "the guide button is nothing");

    SDL_Event motion{};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    check(!t.take(motion, hands) && hands == parts::Hands::Mouse, "the mouse moving touches, and moves nothing");
    SDL_Event right{};
    right.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    right.button.button = SDL_BUTTON_RIGHT;
    check(t.take(right, hands) == Move::Back, "a right-click is Back");

    // A held direction: at once, after 0.42 s, then every 0.12 s closing on 0.045 s.
    input::Translator held;
    check(held.take(key(SDL_EVENT_KEY_DOWN, SDLK_DOWN), hands) == Move::Down, "an arrow moves at once");
    check(!held.take(key(SDL_EVENT_KEY_DOWN, SDLK_DOWN, true), hands), "the keyboard's own repeat is not used");
    check(!held.tick(0.40f), "no repeat before 0.42 s");
    check(held.tick(0.03f) == Move::Down, "the first repeat at 0.42 s");
    check(!held.tick(0.05f), "the next waits");
    check(held.tick(0.08f) == Move::Down, "the second repeat about 0.12 s later");
    int repeats = 0;
    for (int i = 0; i < 300; ++i)
        if (held.tick(0.01f)) ++repeats;
    check(repeats > 40, "a long hold speeds up");
    held.take(key(SDL_EVENT_KEY_UP, SDLK_DOWN), hands);
    check(!held.holding() && !held.tick(1.0f), "letting go stops it");

    // The stick: past half way is a push, back under a third a let-go, and between holds still.
    input::Translator s;
    check(!s.take(axis(SDL_GAMEPAD_AXIS_LEFTY, 9000), hands), "a stick a little off centre moves nothing");
    check(s.take(axis(SDL_GAMEPAD_AXIS_LEFTY, 20000), hands) == Move::Down, "a stick pushed down moves down");
    check(!s.take(axis(SDL_GAMEPAD_AXIS_LEFTY, 14000), hands) && s.holding(), "easing off a little still holds");
    check(!s.take(axis(SDL_GAMEPAD_AXIS_LEFTY, 3000), hands) && !s.holding(), "letting the stick go stops it");
    check(s.take(axis(SDL_GAMEPAD_AXIS_LEFTX, -30000), hands) == Move::Left, "a stick pushed left moves left");

    input::Translator b;
    check(b.take(pad(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_DPAD_UP), hands) == Move::Up, "the d-pad moves");
    b.take(pad(SDL_EVENT_GAMEPAD_BUTTON_UP, SDL_GAMEPAD_BUTTON_DPAD_UP, 2), hands);
    check(b.holding(), "another pad letting go of the same button does not stop this one");
    b.letGo();
    check(!b.holding(), "everything lets go when the window loses focus");
    std::printf("  a controller, the keys and the mouse become moves\n");
}

textline::Line at(const std::string& text, size_t caret) { return textline::Line{text, caret}; }
textline::Line atEnd(const std::string& text) { return textline::Line{text, text.size()}; }
bool same(const textline::Line& l, const std::string& text, size_t caret) { return l.text == text && l.caret == caret; }

void typedLinesKeepWhatTheyShould() {
    using textline::Kind;
    using namespace textline;
    check(same(add(at("", 0), "k7mr4qxt", Kind::Code), "K7MR-4QXT", 9), "a code is tidied as it is typed");
    check(add(atEnd("K7MR-4QXT"), "9hpv", Kind::Code).text == "K7MR-4QXT-9HPV", "a code is finished");
    check(add(at("", 0), " K7MR-4QXT-9HPV\n", Kind::Code).text == "K7MR-4QXT-9HPV", "a pasted code is tidied");
    check(same(backspace(atEnd("K7MR-4"), Kind::Code), "K7MR", 4), "deleting after a dash takes the letter and the dash");
    check(ready("K7MR-4QXT-9HPV", Kind::Code) && !ready("K7MR-4QXT", Kind::Code), "only a whole code is ready");

    check(add(at("", 0), "  linkup.example.org:7845\r\n", Kind::Address).text == "linkup.example.org:7845",
          "a pasted address loses its spaces and line ends");
    check(add(atEnd("linkup.example.org"), ":7845", Kind::Address).text == "linkup.example.org:7845", "an address grows");
    check(add(at("", 0), std::string(400, 'a'), Kind::Address).text.size() == kMaxAddress, "an address has a limit");

    check(add(at("", 0), "  alex", Kind::Name).text == "alex", "a name does not start with spaces");
    check(add(atEnd("al"), "ex\n", Kind::Name).text == "alex", "a line end is not part of a name");
    check(same(backspace(atEnd("Zo\xc3\xab"), Kind::Name), "Zo", 2), "a letter of two bytes goes whole");
    check(backspace(atEnd("\xf0\x9f\x8e\xae"), Kind::Name).text.empty(), "a letter of four bytes goes whole");
    check(add(atEnd(std::string(127, 'a')), "\xc3\xab", Kind::Name).text.size() == 127, "a name stops at 128 bytes, whole");
    check(ready("alex ", Kind::Name) && !ready("   ", Kind::Name), "a name is ready when it has letters");
    std::printf("  typed lines keep what they should\n");
}

void theCaretIsWhereTypingGoes() {
    using textline::Kind;
    using namespace textline;
    check(same(add(at("linkup.exmple.org", 9), "a", Kind::Address), "linkup.example.org", 10),
          "a letter goes in at the caret, and the caret after it");
    check(same(add(at("linkup.org", 7), "example.", Kind::Address), "linkup.example.org", 15), "a paste goes in at the caret");
    check(same(backspace(at("linkup.exaample.org", 10), Kind::Address), "linkup.example.org", 9),
          "Backspace takes the letter before the caret");
    check(same(forwardDelete(at("linkup.exaample.org", 9), Kind::Address), "linkup.example.org", 9),
          "Delete takes the letter after the caret");
    check(same(backspace(at("alex", 0), Kind::Name), "alex", 0), "Backspace at the start takes nothing");
    check(same(forwardDelete(atEnd("alex"), Kind::Name), "alex", 4), "Delete at the end takes nothing");
    check(same(add(at(std::string(kMaxAddress, 'a'), 5), "b", Kind::Address), std::string(kMaxAddress, 'a'), 5),
          "a full address takes nothing more, and keeps its end");

    // Zoë: the ë is two bytes.
    const std::string zoe = "Zo\xc3\xab";
    check(step(at(zoe, 2), 1, Kind::Name).caret == 4, "Right steps over a letter of two bytes whole");
    check(step(atEnd(zoe), 1, Kind::Name).caret == 4, "Right stops at the end");
    check(step(atEnd(zoe), -1, Kind::Name).caret == 2, "Left steps back over it whole");
    check(step(at(zoe, 0), -1, Kind::Name).caret == 0, "Left stops at the start");
    check(moveTo(at(zoe, 0), 3, Kind::Name).caret == 2, "a caret is never inside a letter");
    check(moveTo(at(zoe, 0), std::string::npos, Kind::Name).caret == 4, "End is the end");
    check(same(backspace(at("A Bob", 1), Kind::Name), "Bob", 0), "a name left starting with a space loses it");
    check(same(add(at("Bob", 0), " ", Kind::Name), "Bob", 0), "a name does not start with a typed space");
    check(same(add(at(std::string(128, 'a'), 3), "b", Kind::Name), std::string(128, 'a'), 3),
          "a full name takes nothing more, and keeps its end");

    // A code's caret counts letters: the dashes are the code's.
    check(same(add(at("K7MR-4QXT", 4), "z", Kind::Code), "K7MR-Z4QX-T", 6),
          "a letter in a code's middle regroups it, and the caret is after that letter");
    check(same(add(at("K7MR-4QXT", 4), "ab3cd", Kind::Code), "K7MR-AB3C-4QXT", 9),
          "a paste in a code's middle takes what fits, and keeps the end");
    check(same(add(at("K7MR-4QXT-9HPV", 4), "z", Kind::Code), "K7MR-4QXT-9HPV", 4), "a whole code takes no more");
    check(same(backspace(at("K7MR-4QXT", 4), Kind::Code), "K7M4-QXT", 3), "Backspace in a code regroups it");
    check(same(backspace(at("K7MR-4QXT", 6), Kind::Code), "K7MR-QXT", 4), "Backspace after a dash takes the letter before it");
    check(same(forwardDelete(at("K7MR-4QXT", 4), Kind::Code), "K7MR-QXT", 4), "Delete before a dash takes the letter after it");
    check(step(at("K7MR-4QXT", 4), 1, Kind::Code).caret == 6, "Right steps over a dash and a letter");
    check(step(at("K7MR-4QXT", 6), -1, Kind::Code).caret == 4, "Left steps back to before the dash");
    check(moveTo(at("K7MR-4QXT", 0), 5, Kind::Code).caret == 4, "a click just after a dash is before it");
    std::printf("  the caret is where typing goes\n");
}

void settingsAreKept(const std::string& root) {
    const std::string folder = root + "/settings";
    guest::Settings none = guest::readSettings(folder);
    check(none.colour == "purple" && !none.fullScreen && none.controller.empty() && !none.appearOffline,
          "no file gives the defaults");
    guest::Settings s;
    s.colour = "sunset";
    s.fullScreen = true;
    s.controller = "030000005e040000e002000000006800";
    s.appearOffline = true;
    s.startup = true;
    s.saidTray = true;
    check(!none.startup && !none.saidTray, "Startup is off at first, and nothing has been said");
    check(guest::keepSettings(folder, s), "the settings were not written");
    const guest::Settings back = guest::readSettings(folder);
    check(back.colour == "sunset" && back.fullScreen && back.controller == s.controller && back.appearOffline,
          "the settings did not read back as kept");
    check(back.startup && back.saidTray, "Startup and the tray's notice did not read back as kept");
    cab::writeBytes(folder + "/settings.json", std::string("{ not json"));
    check(guest::readSettings(folder).colour == "purple", "a broken file gives the defaults");
    cab::writeBytes(folder + "/settings.json", std::string("{\"full_screen\": true}"));
    const guest::Settings some = guest::readSettings(folder);
    check(some.fullScreen && some.colour == "purple", "a file with some settings keeps those and defaults the rest");
    std::printf("  the settings are kept\n");
}

// The console's rule (linkupStatusWord): fifteen minutes with nobody, or the screen off, is away.
void idleIsTheConsolesRule() {
    using idle::Seen;
    constexpr double kLong = idle::kAwaySeconds;
    {
        idle::Rule r;
        check(!r.away(Seen{2.0, false}, 100.0), "a computer touched two seconds ago is not away");
        check(!r.away(Seen{kLong - 1.0, false}, kLong + 100.0), "fourteen minutes and more is not away yet");
        check(r.away(Seen{kLong + 1.0, false}, kLong + 101.0), "fifteen minutes with nobody is away");
        check(!r.away(Seen{0.5, false}, kLong + 102.0), "the next touch is back");
        check(!r.away(Seen{kLong + 5.0, false}, 1.0), "a controller in the app counts as a touch");
    }
    {
        idle::Rule r;
        check(!r.away(Seen{1.0, false}, 1.0), "somebody at it");
        // The keys that lock the screen are the last touch before it goes off.
        check(r.away(Seen{0.1, true}, 2.0), "a locked screen is away at once");
        check(r.away(Seen{1.1, true}, 3.0), "and stays away while it is locked");
        check(r.away(Seen{1.2, false}, 3.1), "unlocked by itself, with no touch, is still away");
        check(r.away(Seen{1.3, false}, 3.2), "still away until somebody touches something");
        check(!r.away(Seen{0.2, false}, 3.3), "a touch after it is back");
    }
    {
        idle::Rule r;
        check(!r.away(Seen{5.0, false}, 5.0), "somebody at it");
        check(r.away(Seen{6.0, true}, 6.0), "the computer going to sleep is away at once");
        check(r.away(Seen{0.3, true}, 7.0), "a password typed at the lock screen is a touch, though still locked");
        check(!r.away(Seen{1.0, false}, 8.0), "so unlocking is back at once");
    }
    {
        idle::Rule r;
        check(!r.away(Seen{-1.0, false}, 10.0), "where the computer cannot say, the app's own presses count");
        check(r.away(Seen{-1.0, false}, kLong + 1.0), "and fifteen minutes of none is away");
    }
    std::printf("  idle is the console's rule\n");
}

// Startup's entry starts this program where it is, with --startup, and goes when it is turned off.
void startupIsTheComputersOwn(const std::string& root) {
    const std::string program = "/Applications/Tom & Jerry/2pstart";
    const std::string agent = startup::agent(program);
    check(agent.find("<string>io.github.mmagtech.2pstart</string>") != std::string::npos, "the agent has its label");
    check(agent.find("<string>/Applications/Tom &amp; Jerry/2pstart</string>") != std::string::npos,
          "the agent names the program, escaped");
    check(agent.find("<string>--startup</string>") != std::string::npos, "the agent starts it with --startup");
    check(agent.find("<key>RunAtLoad</key>\n\t<true/>") != std::string::npos, "the agent runs at login");
    check(agent.find("KeepAlive") == std::string::npos, "the agent never starts it again by itself");
    check(startup::runValue("C:\\Program Files\\2P Start\\2pstart.exe") ==
              "\"C:\\Program Files\\2P Start\\2pstart.exe\" --startup",
          "the Run value quotes the path and adds --startup");

    const std::string folder = root + "/LaunchAgents";
    const std::string file = folder + "/io.github.mmagtech.2pstart.plist";
    std::string why;
    check(startup::setAgent(folder, program, true, why), "the agent was not written: " + why);
    const std::vector<uint8_t> held = cab::readBytes(file);
    check(std::string(held.begin(), held.end()) == agent, "the agent written is not the agent");
    check(startup::setAgent(folder, "/elsewhere/2pstart", true, why), "the agent was not moved: " + why);
    const std::vector<uint8_t> moved = cab::readBytes(file);
    check(std::string(moved.begin(), moved.end()).find("/elsewhere/2pstart") != std::string::npos,
          "setting it on again moves it to where the program is");
    check(startup::setAgent(folder, program, false, why) && !std::filesystem::exists(file), "turned off, the agent goes");
    check(startup::setAgent(folder, program, false, why), "turning it off twice is no trouble");

    check(tray::folderKey("/Volumes/Friend/CabinetOS 2P Start") ==
              tray::folderKey("/Volumes/Friend/CabinetOS 2P Start"),
          "a folder has the same name each time");
    check(tray::folderKey("/tmp/one") != tray::folderKey("/tmp/two"), "two folders have two names");
    check(tray::folderKey("").size() == 16, "a folder's name is sixteen letters");
    std::printf("  Startup is the computer's own\n");
}

// A friend's console seals an invite for this app, as the console does (Conductor::cmdInvite).
std::string sealedInvite(const noise::KeyPair& host, const noise::Key& to, const std::string& invite,
                         const std::string& from, const std::string& me, const std::string& game, uint64_t n = 1,
                         int64_t expires = 0) {
    box::Contents c;
    c.type = "invite";
    c.n = n;
    c.expires = expires ? expires : std::time(nullptr) + 1800;
    c.from = from;
    c.to = me;
    c.stream = 1;
    c.game = game;
    const std::string text = box::write(c);
    noise::Bytes sealed;
    box::seal(host, to, invite, noise::Bytes(text.begin(), text.end()), sealed);
    return hexutil::toBase64(sealed);
}

void friendsAndInvitesAreRead() {
    noise::KeyPair me, riley, sam;
    check(noise::newKeyPair(me) && noise::newKeyPair(riley) && noise::newKeyPair(sam), "keys could not be made");
    const std::string invite = "00112233445566778899aabbccddeeff";
    const std::string sealed = sealedInvite(riley, me.pub, invite, "Riley", "alex", "Burnout 3");

    json_object* answer = json_object_new_object();
    json_object* list = json_object_new_array();
    auto person = [&](const char* id, const std::string& name, const char* server, const char* status,
                      const noise::KeyPair* key) {
        json_object* f = json_object_new_object();
        jsonutil::set(f, "id", id);
        jsonutil::set(f, "username", name);
        jsonutil::set(f, "server", server);
        jsonutil::set(f, "status", status);
        jsonutil::set(f, "stream", 1);
        if (key) jsonutil::set(f, "key", hexutil::toBase64(std::vector<uint8_t>(key->pub.begin(), key->pub.end())));
        json_object_array_add(list, f);
        return f;
    };
    json_object* r = person("p1", "Riley", "Riley's", "online", &riley);
    json_object* inv = json_object_new_object();
    jsonutil::set(inv, "id", invite);
    jsonutil::set(inv, "sealed", sealed);
    json_object_object_add(r, "invite", inv);
    person("p2", "Sam\x1b[31m", "", "linkedup", &sam);
    person("p3", "Maya", "", "offline", nullptr);
    person("", "nobody", "", "online", nullptr);
    json_object_object_add(answer, "friends", list);

    const std::vector<guest::Listed> friends = guest::readFriends(answer);
    json_object_put(answer);
    check(friends.size() == 3, "a friend with no id was kept");
    if (friends.size() != 3) return;
    check(friends[0].hasKey && friends[0].inviteId == invite, "Riley's key and invite were not read");
    check(friends[1].name == "Sam[31m" && friends[1].username == "Sam\x1b[31m",
          "a name is cleaned for the screen and kept as sent for its box");
    check(!friends[2].hasKey, "a friend with no key has none");

    box::Reader reader;
    const int64_t now = std::time(nullptr);
    check(guest::openInvite(me, "alex", friends[0], reader, now) == "Burnout 3", "Riley's invite did not open");
    check(guest::openInvite(me, "alex", friends[0], reader, now).empty(), "the same box was taken twice");

    box::Reader other;
    guest::Listed fake = friends[0];
    fake.key = sam.pub;
    check(guest::openInvite(me, "alex", fake, other, now).empty(), "a box opened under the wrong friend's key");
    guest::Listed notMine = friends[0];
    box::Reader third;
    check(guest::openInvite(me, "maya", notMine, third, now).empty(), "a box for somebody else was taken");
    guest::Listed old = friends[0];
    old.inviteSealed = sealedInvite(riley, me.pub, invite, "Riley", "alex", "Burnout 3", 2, now - 10);
    box::Reader fourth;
    check(guest::openInvite(me, "alex", old, fourth, now).empty(), "an expired box was taken");
    guest::Listed elsewhere = friends[0];
    elsewhere.inviteId = "ffeeddccbbaa99887766554433221100";
    box::Reader fifth;
    check(guest::openInvite(me, "alex", elsewhere, fifth, now).empty(), "a box opened for another invite");
    std::printf("  friends are read, and invites opened\n");
}

// The relay as webrtcbin takes it, as the console's turnUri makes it: a login that says anything is
// written so it cannot end the address early.
void theRelayIsWrittenForWebrtcbin() {
    check(guest::turnUri("turn:relay.example.org:3478?transport=udp", "1767225600:ab12", "p/w+=") ==
              "turn://1767225600%3Aab12:p%2Fw%2B%3D@relay.example.org:3478",
          "a relay's address and login were not written as webrtcbin takes them");
    check(guest::turnUri("turn:[2001:db8::1]:3478", "u", "p") == "turn://u:p@[2001:db8::1]:3478", "an IPv6 relay was spoilt");
    check(guest::turnUri("stun:relay.example.org:3478", "u", "p").empty(), "something that is not a relay was taken as one");
    std::printf("  the relay is written for webrtcbin\n");
}

}  // namespace

int main() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("2pstart-app-test-" + std::to_string(static_cast<long long>(std::chrono::steady_clock::now().time_since_epoch().count())));
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::u8string u8 = root.generic_u8string();
    const std::string rootText(u8.begin(), u8.end());

    focusMovesAsOnTheConsole();
    handsBecomeMoves();
    typedLinesKeepWhatTheyShould();
    theCaretIsWhereTypingGoes();
    settingsAreKept(rootText);
    idleIsTheConsolesRule();
    startupIsTheComputersOwn(rootText);
    friendsAndInvitesAreRead();
    theRelayIsWrittenForWebrtcbin();

    std::filesystem::remove_all(root, ignored);
    if (failures > 0) {
        std::fprintf(stderr, "%d failed\n", failures);
        return 1;
    }
    std::printf("app-test: all passed\n");
    return 0;
}
