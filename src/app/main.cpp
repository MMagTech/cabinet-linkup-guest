// 2pstart: CabinetOS 2P Start, the app. A window holding the console's 1920 by 1080 canvas, fitted
// to whatever size it is, and the loop that runs it. What is on screen and what each press does is
// app.cpp's.
//
//   2pstart                     the app, keeping what it holds in the person's own folder (paths.h)
//   2pstart --folder <path>     the same, kept in that folder instead: for trying it as a second friend
//   2pstart --try <file>        steps from a file played into it, with pictures saved (trying.h)
//
// The loop draws at the screen's rate while something moves, and otherwise sleeps until something
// happens: a press, a window change, or the server's threads saying they have news.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <curl/curl.h>

#include "app/app.h"
#include "app/trying.h"
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

int fail(const char* what, const std::string& why) {
    std::fprintf(stderr, "2pstart: %s: %s\n", what, why.c_str());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "CabinetOS 2P Start", (std::string(what) + ": " + why).c_str(),
                             nullptr);
    return 1;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string folder = guest::dataFolder();
    std::string tryFile;
    for (int i = 1; i < argc; i += 2) {
        const std::string flag = argv[i];
        if (i + 1 < argc && flag == "--folder") {
            folder = argv[i + 1];
        } else if (i + 1 < argc && flag == "--try") {
            tryFile = argv[i + 1];
        } else {
            std::fprintf(stderr, "usage: 2pstart [--folder <path>] [--try <file>]\n");
            return 2;
        }
    }
    Trying trying;
    std::string why;
    if (!tryFile.empty() && !trying.load(tryFile, why)) {
        std::fprintf(stderr, "2pstart: %s\n", why.c_str());
        return 2;
    }

    SDL_SetAppMetadata("CabinetOS 2P Start", TWOPSTART_VERSION, "io.github.mmagtech.2pstart");
    // Steps played in from a file take nothing from whoever is at the computer: the window does not
    // come to the front, and their own presses are not mixed in.
    const bool tryingIt = !tryFile.empty();
    if (tryingIt) SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "0");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) return fail("no window", SDL_GetError());
    curl_global_init(CURL_GLOBAL_DEFAULT);

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("CabinetOS 2P Start", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
                                     &window, &renderer))
        return fail("no window", SDL_GetError());
    SDL_SetWindowMinimumSize(window, 640, 360);
    SDL_SetRenderVSync(renderer, 1);
    const Uint32 wake = SDL_RegisterEvents(1);

    {
        bool running = true;
        App app(window, renderer, fontsFolder(), folder, wake);
        if (!app.start(why)) return fail("could not start", why);
        const auto handle = [&](const SDL_Event& e) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            else if (!tryingIt || !fromHands(e)) app.handle(e);
        };

        Uint64 last = SDL_GetTicksNS();
        while (running) {
            SDL_Event e;
            // Asleep until something happens, unless something on screen moves.
            if (!app.busy() && !tryingIt && SDL_WaitEventTimeout(&e, 1000)) handle(e);
            while (SDL_PollEvent(&e)) handle(e);
            const Uint64 now = SDL_GetTicksNS();
            const float dt = static_cast<float>(static_cast<double>(now - last) / 1e9);
            last = now;
            app.tick(std::min(dt, 0.25f));
            if (tryingIt && !trying.play(dt, app, renderer)) running = false;
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
