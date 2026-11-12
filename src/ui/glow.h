// The bars around the host's game, and the console's letterbox glow over them (CabinetOS #692).
//
// WHY THIS APP DRAWS THEM. A game narrower than the screen leaves bars beside it, and the console
// lights them with a faint white glow. The stream carries the host's bars, but the encoder keeps
// nothing that faint and turns the glow's fade into flat, blocky steps. So the host says where its
// game sits on its screen, and the friend paints the bars black and draws the same glow over them,
// smooth, stopping exactly at the picture (decision 2026-11-10, linkup-friend-draws-the-glow). With
// no word from the host, the stream's own bars stay, as before.
//
// THE CONSOLE'S NUMBERS, from its kGlowFS (shaders.inc) and LinkUpPlayScreen::drawPlace: white, at
// Strong (a peak of 0.04, always on: decision 2026-10-04); a ramp of peak * (1 - t^1.7) from the
// picture's edge to the screen's; the side bars the full height and the top and bottom bars only
// the picture's width; a still noise dither in both the black and the glow, so a long near-black
// ramp does not band on an 8-bit screen; the edges on whole pixels of this screen.
//
// The console does it in a shader each frame. SDL's renderer has none, so this works out the same
// pixels once, into a texture, and makes it again only when the place or the window's size changes.

#pragma once

#include <SDL3/SDL.h>

namespace ui {

// Strong, the console's one glow.
constexpr float kGlowPeak = 0.04f;

class Glow {
public:
    explicit Glow(SDL_Renderer* renderer) : renderer_(renderer) {}
    ~Glow();
    Glow(const Glow&) = delete;
    Glow& operator=(const Glow&) = delete;

    // `screen` is where the host's whole screen is drawn, in this window's pixels; `place` is where
    // its game sits on that screen, in the console's canvas points (1920 by 1080). Draws the bars
    // and the glow around the game, over whatever is there.
    void draw(const SDL_FRect& screen, const SDL_FRect& place);

private:
    SDL_Renderer* renderer_;
    SDL_Texture* texture_ = nullptr;
    // What the texture was made for: the screen's pixels, and the game's edges on them.
    int x_ = 0, y_ = 0, w_ = 0, h_ = 0;
    int left_ = 0, top_ = 0, right_ = 0, bottom_ = 0;
};

}  // namespace ui
