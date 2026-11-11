// 2pstart-gallery: every screen of 2P Start with made-up friends, for judging how it looks before
// any of it is wired to a server or a stream. The console has the same for its own LinkUp screens
// (--linkup-gallery).
//
//   2pstart-gallery                    a window: Left and Right step through, F is full screen, Esc leaves
//   2pstart-gallery --shots <folder>   a picture of each screen, 1920 by 1080, and no window
//
// The made-up friends are the console's own: Riley hosts and has invited you to Burnout 3.

#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "linkuprules.h"
#include "screens/screens.h"
#include "ui/draw.h"
#include "ui/look.h"
#include "ui/parts.h"

namespace {

using linkup::Status;

struct Scene {
    const char* file;    // the picture's name
    const char* says;    // what it shows, for the window's corner
    std::function<void(ui::Canvas&, float clock)> draw;
};

const char* kHost = "Riley";
const char* kGame = "Burnout 3";
const char* kAddress = "linkup.example.org:7845";

linkup::State listOf(std::vector<linkup::Friend> friends) {
    linkup::State s;
    s.on = s.paired = true;
    s.friends = std::move(friends);
    return s;
}

// The console's own gallery list, so this screen can be held beside the console's.
linkup::State everyone(bool invited) {
    return listOf({{"Riley", "", invited ? Status::InvitedYou : Status::Online, invited ? kGame : ""},
                   {"Sam", "", Status::InGame, ""},
                   {"Alex", "", Status::Online, ""},
                   {"Jordan", "", Status::Online, ""},
                   {"Morgan", "", Status::Online, ""},
                   {"Casey", "", Status::Online, ""},
                   {"Maya", "", Status::Offline, ""},
                   {"Theo", "", Status::Offline, ""}});
}

// What most people who use this will see: the one friend who invited them.
linkup::State oneFriend(Status status) { return listOf({{"Riley", "", status, status == Status::InvitedYou ? kGame : ""}}); }

linkup::State instead(const std::string& words) {
    linkup::State s = listOf({});
    s.instead = words;
    return s;
}

void friends(ui::Canvas& c, const linkup::State& state, int focus = 0) {
    screens::FriendsView v;
    v.state = state;
    v.focus = focus;
    screens::drawFriends(c, v);
}

void firstRun(ui::Canvas& c, float clock, int focus, bool filled, bool connecting = false) {
    screens::FirstRunView v;
    if (filled) {
        v.address = "linkup.example.org:7845";
        v.code = "K7MR-4QXT-9HPV";
        v.name = "alex";
    }
    v.focus = focus;
    v.connecting = connecting;
    v.clock = clock;
    screens::drawFirstRun(c, v);
}

void waiting(ui::Canvas& c, float clock, const std::string& title, const std::string& game, bool resumes = false,
             parts::Hands hands = parts::Hands::Controller) {
    screens::WaitingView v;
    v.title = title;
    v.game = game;
    v.resumes = resumes;
    v.hands = hands;
    v.clock = clock;
    screens::drawWaiting(c, v);
}

// Every LinkUp question is one width, as on the console (linkup.cpp, linkupPanel).
parts::Panel question(const std::string& title, const std::string& detail, std::vector<std::string> options, int focused) {
    parts::Panel p;
    p.title = title;
    p.detail = detail;
    p.options = std::move(options);
    p.focused = focused;
    p.width = parts::kLinkUpPanelWidth;
    return p;
}

parts::Panel said(const std::string& title, const char* after, parts::Hands hands = parts::Hands::Controller,
                  const std::string& detail = "") {
    parts::Panel p;
    p.title = title;
    p.detail = detail;
    p.prompt = parts::prompt(hands, false, after);
    p.width = parts::kLinkUpPanelWidth;
    return p;
}

screens::SettingsView settingsOf(look::Colour colour, int focus) {
    screens::SettingsView v;
    v.name = "alex";
    v.colour = static_cast<int>(colour);
    v.controllers = {"Xbox Wireless Controller"};
    v.server = kAddress;
    v.focus = focus;
    return v;
}

void settings(ui::Canvas& c, look::Colour colour, int focus) { screens::drawSettings(c, settingsOf(colour, focus)); }

std::vector<Scene> scenes() {
    using parts::Tone;
    return {
        {"01-first-run", "First run", [](ui::Canvas& c, float t) { firstRun(c, t, 0, false); }},
        {"02-first-run-filled", "First run, filled in", [](ui::Canvas& c, float t) { firstRun(c, t, 3, true); }},
        {"03-first-run-connecting", "First run, pairing", [](ui::Canvas& c, float t) { firstRun(c, t, 3, true, true); }},
        {"04-first-run-wrong-code", "First run, a wrong code",
         [](ui::Canvas& c, float t) {
             firstRun(c, t, 1, true);
             parts::notice(c, "Wrong or expired code", Tone::Problem);
         }},
        {"05-friends-one", "The list, with the one friend who invited you",
         [](ui::Canvas& c, float) { friends(c, oneFriend(Status::Online)); }},
        {"06-friends-many", "The list, with the console's eight friends",
         [](ui::Canvas& c, float) { friends(c, everyone(true)); }},
        {"07-invite-arrives", "An invite arrives",
         [](ui::Canvas& c, float) {
             friends(c, oneFriend(Status::InvitedYou));
             parts::notice(c, linkup::invitedYou(kHost), Tone::Done);
         }},
        {"08-join-or-decline", "Join or Decline",
         [](ui::Canvas& c, float) {
             friends(c, oneFriend(Status::InvitedYou));
             parts::panel(c, question(linkup::joinQuestion(kHost), kGame, {"Join", "Decline"}, 0));
         }},
        {"09-waiting", "Waiting for the host", [](ui::Canvas& c, float t) { waiting(c, t, linkup::waitingFor(kHost), kGame); }},
        {"10-game", "The game", [](ui::Canvas& c, float t) { screens::drawPictureStandIn(c, kGame, t); }},
        {"11-host-paused", "The host paused",
         [](ui::Canvas& c, float t) { waiting(c, t, linkup::pausedBy(kHost), kGame, true); }},
        {"12-paused-here", "You paused",
         [](ui::Canvas& c, float t) {
             screens::drawPictureStandIn(c, kGame, t);
             parts::panel(c, question("Paused", "", {"Resume", "Leave"}, 0));
         }},
        {"13-reconnecting", "Reconnecting", [](ui::Canvas& c, float t) { waiting(c, t, linkup::reconnectingTo(kHost), kGame); }},
        {"14-still-playing", "Idle for ten minutes",
         [](ui::Canvas& c, float t) {
             screens::drawPictureStandIn(c, kGame, t);
             parts::panel(c, said(linkup::kStillPlaying, "to resume"));
         }},
        {"15-slow-connection", "A slow connection",
         [](ui::Canvas& c, float t) {
             screens::drawPictureStandIn(c, kGame, t);
             parts::notice(c, linkup::kTooSlow, Tone::Problem);
         }},
        {"16-leave-question", "Leaving",
         [](ui::Canvas& c, float t) {
             screens::drawPictureStandIn(c, kGame, t);
             parts::panel(c, question(linkup::leaveQuestion(kHost), "", {"Leave", "Cancel"}, 1));
         }},
        {"17-session-ended", "The host ended the session",
         [](ui::Canvas& c, float) {
             friends(c, oneFriend(Status::Online));
             parts::panel(c, said(linkup::endedSession(kHost), "to continue"));
         }},
        {"18-not-available", "The host isn't available",
         [](ui::Canvas& c, float) {
             friends(c, oneFriend(Status::Online));
             parts::panel(c, said(linkup::notAvailable(kHost), "to continue"));
         }},
        {"19-no-friends", "No friends", [](ui::Canvas& c, float) { friends(c, listOf({})); }},
        {"20-cant-connect", "The server cannot be reached",
         [](ui::Canvas& c, float) { friends(c, instead(linkup::kCantConnect)); }},
        {"21-removed", "Removed from the server", [](ui::Canvas& c, float) { friends(c, instead(linkup::kConsoleRemoved)); }},
        {"22-needs-update", "The app is too old for the server",
         [](ui::Canvas& c, float) { friends(c, instead("2P Start needs an update")); }},
        {"23-server-needs-update", "The server is too old for the app",
         [](ui::Canvas& c, float) { friends(c, instead(linkup::kServerNeedsUpdate)); }},
        {"24-no-controller", "No controller",
         [](ui::Canvas& c, float) {
             friends(c, oneFriend(Status::Online));
             parts::notice(c, "Connect a controller to play", Tone::Problem);
         }},
        {"25-settings", "Settings", [](ui::Canvas& c, float) { settings(c, look::Colour::Purple, 1); }},
        {"26-colour-teal", "Settings, in Teal",
         [](ui::Canvas& c, float) {
             c.setLook(look::of(look::Colour::Teal));
             settings(c, look::Colour::Teal, 1);
         }},
        {"27-colour-sunset", "The list, in Sunset",
         [](ui::Canvas& c, float) {
             c.setLook(look::of(look::Colour::Sunset));
             friends(c, everyone(true));
         }},
        {"28-colour-sky-panel", "Join or Decline, in Sky",
         [](ui::Canvas& c, float) {
             c.setLook(look::of(look::Colour::Sky));
             friends(c, oneFriend(Status::InvitedYou));
             parts::panel(c, question(linkup::joinQuestion(kHost), kGame, {"Join", "Decline"}, 0));
         }},
        // The app's own: typing, the hints for each kind of hands, Settings' rows that do
        // something, and what Join says until the game can be shown.
        {"29-first-run-typing", "First run, typing the code",
         [](ui::Canvas& c, float t) {
             screens::FirstRunView v;
             v.address = "linkup.example.org:7845";
             v.code = "K7MR-4Q";
             v.focus = 1;
             v.clock = v.still = t;
             screens::drawFirstRun(c, v);
         }},
        // The caret moved back into a line, by the arrow keys or a click, to fix a letter there.
        {"29a-first-run-caret", "First run, the caret back in the address",
         [](ui::Canvas& c, float t) {
             screens::FirstRunView v;
             v.address = "linkup.exmple.org:7845";
             v.caret = 9;   // after "linkup.ex"
             v.focus = 0;
             v.clock = v.still = t;
             screens::drawFirstRun(c, v);
         }},
        {"29b-first-run-code-caret", "First run, the caret before a dash in the code",
         [](ui::Canvas& c, float t) {
             screens::FirstRunView v;
             v.address = kAddress;
             v.code = "K7MR-4QXT-9HPV";
             v.caret = 4;   // after "K7MR"
             v.focus = 1;
             v.clock = v.still = t;
             screens::drawFirstRun(c, v);
         }},
        {"30-first-run-refused", "First run, the server refused",
         [](ui::Canvas& c, float t) {
             firstRun(c, t, 3, true);
             parts::notice(c, "That server refused", Tone::Problem);
         }},
        {"31-waiting-keyboard", "Waiting, after the keyboard was used",
         [](ui::Canvas& c, float t) { waiting(c, t, linkup::pausedBy(kHost), kGame, true, parts::Hands::Keyboard); }},
        {"32-waiting-mouse", "Waiting, after the mouse was used",
         [](ui::Canvas& c, float t) { waiting(c, t, linkup::pausedBy(kHost), kGame, true, parts::Hands::Mouse); }},
        {"33-ended-keyboard", "The host ended the session, keyboard",
         [](ui::Canvas& c, float) {
             friends(c, oneFriend(Status::Online));
             parts::panel(c, said(linkup::endedSession(kHost), "to continue", parts::Hands::Keyboard));
         }},
        {"34-ended-mouse", "The host ended the session, mouse",
         [](ui::Canvas& c, float) {
             friends(c, oneFriend(Status::Online));
             parts::panel(c, said(linkup::endedSession(kHost), "to continue", parts::Hands::Mouse));
         }},
        {"35-cant-play-yet", "Join, before the game can be shown",
         [](ui::Canvas& c, float) {
             friends(c, oneFriend(Status::Online));
             parts::panel(c, said("Not supported yet", "to continue"));
         }},
        {"36-settings-name", "Settings, typing a new name",
         [](ui::Canvas& c, float t) {
             screens::SettingsView v = settingsOf(look::Colour::Purple, screens::setting::Name);
             v.name = "alex_on_pc";
             v.typingName = true;
             v.still = t;
             screens::drawSettings(c, v);
         }},
        {"36a-settings-name-caret", "Settings, the caret inside the name",
         [](ui::Canvas& c, float t) {
             screens::SettingsView v = settingsOf(look::Colour::Purple, screens::setting::Name);
             v.name = "alex_on_pc";
             v.typingName = true;
             v.caret = 4;   // after "alex"
             v.still = t;
             screens::drawSettings(c, v);
         }},
        {"37-settings-controller", "Settings, two controllers",
         [](ui::Canvas& c, float) {
             screens::SettingsView v = settingsOf(look::Colour::Purple, screens::setting::Controller);
             v.controllers = {"Xbox Wireless Controller", "DualSense Wireless Controller"};
             screens::drawSettings(c, v);
         }},
        {"38-settings-server", "Settings, the server",
         [](ui::Canvas& c, float) {
             settings(c, look::Colour::Purple, screens::setting::Server);
             parts::panel(c, question(kAddress, "", {"Change server", "Leave"}, 0));
         }},
        {"39-credits", "Credits and licenses",
         [](ui::Canvas& c, float) {
             settings(c, look::Colour::Purple, screens::setting::About);
             screens::drawCredits(c, screens::CreditsView{});
         }},
        {"40-credits-end", "Credits and licenses, at the end",
         [](ui::Canvas& c, float) {
             settings(c, look::Colour::Purple, screens::setting::About);
             screens::CreditsView v;
             v.focus = screens::creditCount() - 1;
             v.top = screens::creditCount() - parts::kPanelRows;
             screens::drawCredits(c, v);
         }},
        {"44-about", "About",
         [](ui::Canvas& c, float) {
             settings(c, look::Colour::Purple, screens::setting::About);
             screens::AboutView v;
             v.version = TWOPSTART_VERSION;
             screens::drawAbout(c, v);
         }},
        {"41-settings-leave", "Settings, leaving the server",
         [](ui::Canvas& c, float) {
             settings(c, look::Colour::Purple, screens::setting::Server);
             parts::panel(c, question(std::string("Leave ") + kAddress + "?", "", {"Leave", "Cancel"}, 1));
         }},
        {"42-change-server", "Change server: its address",
         [](ui::Canvas& c, float t) {
             settings(c, look::Colour::Purple, screens::setting::Server);
             parts::FieldPanel p;
             p.title = "Server address";
             p.value = "linkup.example.org:79";
             p.placeholder = "name.example.net:7845";
             p.still = t;
             p.prompt = parts::prompt(parts::Hands::Keyboard, false, "to continue");
             parts::fieldPanel(c, p);
         }},
        {"42a-change-server-long", "Change server: a long address, the caret inside it",
         [](ui::Canvas& c, float t) {
             settings(c, look::Colour::Purple, screens::setting::Server);
             parts::FieldPanel p;
             p.title = "Server address";
             p.value = "cabinet-linkup.friday-games-night.riley-and-sam.example.org:7845";
             p.caret = 8;   // after "cabinet-"
             p.placeholder = "name.example.net:7845";
             p.still = t;
             p.prompt = parts::prompt(parts::Hands::Keyboard, false, "to continue");
             parts::fieldPanel(c, p);
         }},
        {"43-change-server-code", "Change server: another server's code",
         [](ui::Canvas& c, float t) {
             settings(c, look::Colour::Purple, screens::setting::Server);
             parts::FieldPanel p;
             p.title = "Code";
             p.hint = "linkup.example.org:7945";
             p.placeholder = "XXXX-XXXX-XXXX";
             p.still = t;
             p.prompt = parts::prompt(parts::Hands::Keyboard, false, "to continue");
             parts::fieldPanel(c, p);
         }},
    };
}

std::string fontsFolder() {
    const char* base = SDL_GetBasePath();
    return std::string(base ? base : "") + "assets/fonts";
}

int shots(const std::string& folder) {
    std::error_code ignored;
    std::filesystem::create_directories(std::filesystem::path(std::u8string(folder.begin(), folder.end())), ignored);
    SDL_Surface* surface = SDL_CreateSurface(static_cast<int>(ui::kCanvasWidth), static_cast<int>(ui::kCanvasHeight),
                                             SDL_PIXELFORMAT_ARGB8888);
    SDL_Renderer* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    if (!renderer) {
        std::fprintf(stderr, "no renderer: %s\n", SDL_GetError());
        return 1;
    }
    int failed = 0;
    {
        ui::Canvas canvas(renderer, fontsFolder());
        if (!canvas.ready()) {
            std::fprintf(stderr, "no fonts in %s\n", fontsFolder().c_str());
            return 1;
        }
        for (const Scene& scene : scenes()) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            canvas.setLook(ui::Look{});
            scene.draw(canvas, 1.3f);
            SDL_RenderPresent(renderer);
            const std::string file = folder + "/" + scene.file + ".png";
            if (!SDL_SavePNG(surface, file.c_str())) {
                std::fprintf(stderr, "could not save %s: %s\n", file.c_str(), SDL_GetError());
                ++failed;
            } else {
                std::printf("%s\n", file.c_str());
            }
        }
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    return failed == 0 ? 0 : 1;
}

int window() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "no window: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* win = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("CabinetOS 2P Start: the screens", 1280, 720,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &win, &renderer)) {
        std::fprintf(stderr, "no window: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);
    {
        ui::Canvas canvas(renderer, fontsFolder());
        if (!canvas.ready()) {
            std::fprintf(stderr, "no fonts in %s\n", fontsFolder().c_str());
            return 1;
        }
        const std::vector<Scene> all = scenes();
        int at = 0;
        bool full = false, running = true;
        const Uint64 began = SDL_GetTicks();
        while (running) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) running = false;
                if (e.type != SDL_EVENT_KEY_DOWN) continue;
                const int count = static_cast<int>(all.size());
                if (e.key.key == SDLK_ESCAPE) running = false;
                else if (e.key.key == SDLK_RIGHT || e.key.key == SDLK_SPACE) at = (at + 1) % count;
                else if (e.key.key == SDLK_LEFT) at = (at + count - 1) % count;
                else if (e.key.key == SDLK_F) SDL_SetWindowFullscreen(win, full = !full);
            }
            int w = 0, h = 0;
            SDL_GetRenderOutputSize(renderer, &w, &h);
            canvas.setOutput(w, h);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            const Scene& scene = all[static_cast<size_t>(at)];
            canvas.setLook(ui::Look{});
            scene.draw(canvas, static_cast<float>(SDL_GetTicks() - began) / 1000.0f);
            // Which one this is, small in the corner, so a screen can be named when talking about it.
            canvas.text(std::to_string(at + 1) + " of " + std::to_string(all.size()) + "  " + scene.says, 24.0f,
                        ui::kCanvasHeight - 20.0f, ui::TextStyle::Caption1, ui::Color::white(0.35f));
            SDL_RenderPresent(renderer);
        }
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 3 && std::string(argv[1]) == "--shots") return shots(argv[2]);
    if (argc != 1) {
        std::fprintf(stderr, "usage: 2pstart-gallery [--shots <folder>]\n");
        return 2;
    }
    return window();
}
