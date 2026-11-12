// 2pstart: CabinetOS 2P Start, the app. A window holding the console's 1920 by 1080 canvas, fitted
// to whatever size it is, and the loop that runs it. What is on screen and what each press does is
// app.cpp's.
//
//   2pstart                     the app, keeping what it holds in the person's own folder (paths.h)
//   2pstart --folder <path>     the same, kept in that folder instead: for trying it as a second friend
//   2pstart --try <file>        steps from a file played into it, with pictures saved (trying.h)
//   2pstart --startup           as at login (startup.h): running, with no window until it is opened
//   2pstart --check-game        whether this copy can play: what it lacks, a line each, and no window
//
// On Windows, opened from the Start menu or by a double-click, it has nowhere to write what it says
// as it runs, so that goes to 2pstart.log in its folder, and the run before's to 2pstart.old.log.
//
// One runs for each folder: started again, it brings back the one running (tray.h). Its window's
// close button hides it, and the app keeps running in the tray or the Dock until it is quit. In a
// game the close button pauses it or asks to leave instead.
//
// The loop draws at the screen's rate while something moves, and otherwise sleeps until something
// happens: a press, a window change, or the server's threads saying they have news. While a game's
// picture is all there is, it wakes for each new picture or press, and draws a picture as soon as it
// comes, with no wait for the screen (App::live): the console measured that wait at 44 to 50 ms.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <curl/curl.h>

#include "app/app.h"
#include "app/startup.h"
#include "app/tray.h"
#include "app/trying.h"
#include "game/game.h"
#include "guest/paths.h"

namespace {

std::string fontsFolder() {
    const char* base = SDL_GetBasePath();
    return std::string(base ? base : "") + "assets/fonts";
}

// A press, a key, the mouse or a controller: what a person's hands make.
bool fromHands(const SDL_Event& e) {
    switch (e.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_TEXT_INPUT:
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_WHEEL:
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: return true;
        default: return false;
    }
}

// --check-game: everything a game needs that GStreamer lacks, and its plugins that would not load,
// a line each, and nothing when it can play. CI runs it on the installed Windows app and the Mac app on
// a computer with nothing else of GStreamer's (ci/check-setup.ps1, ci/check-app.sh).
int checkGame() {
    game::Game game([] {});
    int found = 0;
    for (const std::string& part : game.lacking()) {
        std::printf("lacks %s\n", part.c_str());
        ++found;
    }
    for (const std::string& plugin : game.unloadable()) {
        std::printf("cannot load %s\n", plugin.c_str());
        ++found;
    }
    std::fflush(stdout);
    return found == 0 ? 0 : 1;
}

// The log, on Windows, when nobody is there to read it as it is written.
void keepLog(const std::string& folder) {
#ifdef _WIN32
    const HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    if (folder.empty() || (err && err != INVALID_HANDLE_VALUE)) return;
    const std::filesystem::path dir(std::u8string(folder.begin(), folder.end()));
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::filesystem::rename(dir / "2pstart.log", dir / "2pstart.old.log", ec);
    if (!_wfreopen((dir / "2pstart.log").c_str(), L"w", stderr)) return;
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    SetStdHandle(STD_ERROR_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stderr))));
#else
    (void)folder;
#endif
}

int fail(const char* what, const std::string& why) {
    std::fprintf(stderr, "2pstart: %s: %s\n", what, why.c_str());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "2P Start", (std::string(what) + ": " + why).c_str(),
                             nullptr);
    return 1;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string folder = guest::dataFolder();
    std::string tryFile;
    bool atLogin = false;
    for (int i = 1; i < argc; ++i) {
        const std::string flag = argv[i];
        if (flag == startup::kFlag) {
            atLogin = true;
        } else if (i + 1 < argc && flag == "--folder") {
            folder = argv[++i];
        } else if (i + 1 < argc && flag == "--try") {
            tryFile = argv[++i];
        } else if (flag == "--check-game") {
            return checkGame();
        } else {
            std::fprintf(stderr, "usage: 2pstart [--folder <path>] [--try <file>] [--startup] [--check-game]\n");
            return 2;
        }
    }
    Trying trying;
    std::string why;
    if (!tryFile.empty() && !trying.load(tryFile, why)) {
        std::fprintf(stderr, "2pstart: %s\n", why.c_str());
        return 2;
    }

    // Already running with this folder: that one comes to the front instead, unless this is the login's.
    if (!folder.empty() && tray::another(folder, atLogin)) return 0;
    keepLog(folder);

    SDL_SetAppMetadata("2P Start", TWOPSTART_VERSION, "io.github.mmagtech.2pstart");
#ifdef _WIN32
    // The window and the taskbar show the program's own icon (cmake/2pstart.rc.in).
    SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON, "1");
    SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON_SMALL, "1");
#endif
    // Opened at login, it has no window, and on a Mac no Dock icon (tray.h).
    if (atLogin) SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1");
    // The close button hides the window (tray.h), and a controller is heard with another program in
    // front, for the idle rule (idle.h).
    SDL_SetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    // Steps played in from a file take nothing from whoever is at the computer: the window does not
    // come to the front, and their own presses are not mixed in.
    const bool tryingIt = !tryFile.empty();
    if (tryingIt) SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "0");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) return fail("no window", SDL_GetError());
    curl_global_init(CURL_GLOBAL_DEFAULT);

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    const SDL_WindowFlags shown = atLogin ? SDL_WINDOW_HIDDEN : 0;
    if (!SDL_CreateWindowAndRenderer("2P Start", 1280, 720,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | shown, &window, &renderer))
        return fail("no window", SDL_GetError());
    SDL_SetWindowMinimumSize(window, 640, 360);
    SDL_SetRenderVSync(renderer, 1);
    const Uint32 wake = SDL_RegisterEvents(1);

    {
        bool running = true;
        App app(window, renderer, fontsFolder(), folder, wake);
        if (!app.start(why)) return fail("could not start", why);
        const auto invites = [&app]() {
            std::vector<tray::Invite> out;
            for (const App::Waiting& w : app.invites()) out.push_back({w.who, w.title, w.game});
            return out;
        };
        tray::Asks asks;
        asks.invites = invites;
        asks.online = [&app]() { return app.online(); };
        asks.setOnline = [&app](bool on) {
            if (app.online() != on) app.flipStatus();
        };
        asks.reply = [&app](const std::string& who, int answer) { app.reply(who, answer); };
        asks.startup = [&app]() { return app.startupOn(); };
        asks.canStartup = [&app]() { return app.keepRunning(); };
        asks.flipStartup = [&app]() { app.flipStartup(); };
        asks.keepRunning = [&app]() { return app.keepRunning(); };
        asks.sayStillRunning = [&app]() { return app.sayStillRunning(); };
        tray::Tray tray(window, folder, std::move(asks));
        app.tellInvites([&tray](const std::string& who, const std::string& line, const std::string& game) {
            return tray.invited(tray::Invite{who, line, game});
        });
        const auto handle = [&](const SDL_Event& e) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            else if (app.closeInGame(e)) return;
            else if (tray.take(e)) return;
            else if (tryingIt && fromHands(e)) return;
            else if (!app.elsewhere(e)) app.handle(e);
        };

        Uint64 last = SDL_GetTicksNS();
        bool liveShown = false;
        while (running) {
            SDL_Event e;
            const bool live = app.live() && !tryingIt;
            // Asleep until something happens, unless something on screen moves. A game's picture
            // wakes it too; a few milliseconds at most, so the pad is read that often.
            if (live) {
                if (SDL_WaitEventTimeout(&e, 4)) handle(e);
            } else if (!app.busy() && !tryingIt && SDL_WaitEventTimeout(&e, 1000)) {
                handle(e);
            }
            while (SDL_PollEvent(&e)) handle(e);
            const Uint64 now = SDL_GetTicksNS();
            const float dt = static_cast<float>(static_cast<double>(now - last) / 1e9);
            last = now;
            app.tick(std::min(dt, 0.25f));
            tray.follow(invites(), app.seenOnline());
            if (tryingIt && !trying.play(dt, app, renderer)) running = false;
            // The picture alone, with no new one: nothing to draw.
            const bool liveNow = app.live() && !tryingIt;
            if (liveNow && liveShown && !app.fresh()) continue;
            liveShown = liveNow;
            app.draw();
            trying.takeShot(renderer);
            SDL_RenderPresent(renderer);
        }
        app.quit();
        // The long request may still be held by the server, on a thread of its own that still uses
        // what the app holds. Nothing is left to save, so the program ends here, before any of it
        // is taken down, rather than waiting for that request.
        SDL_Quit();
        std::_Exit(0);
    }
}
