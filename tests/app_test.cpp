// The app's parts that need no window and no network: where focus goes, what a controller, the
// keys and the mouse ask for, what a typed line keeps, the settings file, and the server's list of
// friends read with each invite's sealed box opened.
//
//   app-test

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

#include "app/input.h"
#include "app/moves.h"
#include "app/textline.h"
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

void typedLinesKeepWhatTheyShould() {
    using textline::Kind;
    check(textline::add("", "k7mr4qxt", Kind::Code) == "K7MR-4QXT", "a code is tidied as it is typed");
    check(textline::add("K7MR-4QXT", "9hpv", Kind::Code) == "K7MR-4QXT-9HPV", "a code is finished");
    check(textline::add("", " K7MR-4QXT-9HPV\n", Kind::Code) == "K7MR-4QXT-9HPV", "a pasted code is tidied");
    check(textline::backspace("K7MR-4", Kind::Code) == "K7MR", "deleting after a dash takes the letter and the dash");
    check(textline::ready("K7MR-4QXT-9HPV", Kind::Code) && !textline::ready("K7MR-4QXT", Kind::Code),
          "only a whole code is ready");

    check(textline::add("", "  linkup.example.org:7845\r\n", Kind::Address) == "linkup.example.org:7845",
          "a pasted address loses its spaces and line ends");
    check(textline::add("linkup.example.org", ":7845", Kind::Address) == "linkup.example.org:7845", "an address grows");
    check(textline::add("", std::string(400, 'a'), Kind::Address).size() == textline::kMaxAddress, "an address has a limit");

    check(textline::add("", "  alex", Kind::Name) == "alex", "a name does not start with spaces");
    check(textline::add("al", "ex\n", Kind::Name) == "alex", "a line end is not part of a name");
    check(textline::backspace("Zo\xc3\xab", Kind::Name) == "Zo", "a letter of two bytes goes whole");
    check(textline::backspace("\xf0\x9f\x8e\xae", Kind::Name).empty(), "a letter of four bytes goes whole");
    check(textline::add(std::string(127, 'a'), "\xc3\xab", Kind::Name).size() == 127, "a name stops at 128 bytes, whole");
    check(textline::ready("alex ", Kind::Name) && !textline::ready("   ", Kind::Name), "a name is ready when it has letters");
    std::printf("  typed lines keep what they should\n");
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
    check(guest::keepSettings(folder, s), "the settings were not written");
    const guest::Settings back = guest::readSettings(folder);
    check(back.colour == "sunset" && back.fullScreen && back.controller == s.controller && back.appearOffline,
          "the settings did not read back as kept");
    cab::writeBytes(folder + "/settings.json", std::string("{ not json"));
    check(guest::readSettings(folder).colour == "purple", "a broken file gives the defaults");
    cab::writeBytes(folder + "/settings.json", std::string("{\"full_screen\": true}"));
    const guest::Settings some = guest::readSettings(folder);
    check(some.fullScreen && some.colour == "purple", "a file with some settings keeps those and defaults the rest");
    std::printf("  the settings are kept\n");
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
    settingsAreKept(rootText);
    friendsAndInvitesAreRead();

    std::filesystem::remove_all(root, ignored);
    if (failures > 0) {
        std::fprintf(stderr, "%d failed\n", failures);
        return 1;
    }
    std::printf("app-test: all passed\n");
    return 0;
}
