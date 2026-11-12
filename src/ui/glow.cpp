// The bars around the host's game and the glow over them (glow.h). Each step names the line of the
// console's kGlowFS (shaders.inc) it follows.

#include "ui/glow.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ui {

namespace {

// How far along a strip goes before it repeats: the noise in it repeats as far apart, which a still
// dither one level deep does not show.
constexpr int kAlong = 256;

// A still noise: a hash of the pixel's place, 0 to 1, as the shader's is. Never animated: a moving
// dither in the corner of a dark room is a shimmer. The shader hashes with a sine, the usual trick on
// a graphics chip; this is the same white noise from whole numbers, many times faster on a
// processor. `seed` makes the two the shader takes.
double noise(uint32_t x, uint32_t y, uint32_t seed) {
    uint32_t v = x * 0x9E3779B1u ^ (y + seed) * 0x85EBCA77u;
    v ^= v >> 16;
    v *= 0x7FEB352Du;
    v ^= v >> 15;
    v *= 0x846CA68Bu;
    v ^= v >> 16;
    return static_cast<double>(v) / 4294967296.0;
}

// The glow's fall from the picture's edge (t = 0) to the screen's (t = 1): a flat start, so the
// light carries further before it drops.
double ramp(double t) { return 1.0 - std::pow(std::clamp(t, 0.0, 1.0), 1.7); }

}  // namespace

Glow::~Glow() {
    for (Strip& s : strips_)
        if (s.texture) SDL_DestroyTexture(s.texture);
}

SDL_Texture* Glow::strip(Side side, int thick) {
    Strip& s = strips_[side];
    if (s.texture && s.thick == thick) return s.texture;
    if (s.texture) SDL_DestroyTexture(s.texture);
    s = Strip{};
    const bool across = side == kLeft || side == kRight;   // the fade runs across the screen, left or right
    const int w = across ? thick : kAlong, h = across ? kAlong : thick;
    // The fade at each distance from the game's edge, in pixels: t is 0 there and 1 at the screen's
    // edge, measured from each pixel's middle.
    std::vector<double> fade(static_cast<size_t>(thick));
    for (int at = 0; at < thick; ++at) {
        const int d = side == kLeft || side == kTop ? thick - 1 - at : at;
        fade[static_cast<size_t>(at)] = ramp((d + 0.5) / thick);
    }
    std::vector<uint8_t> pixels(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            const double r = fade[static_cast<size_t>(across ? i : j)];
            const double n1 = noise(static_cast<uint32_t>(i), static_cast<uint32_t>(j), 1);
            const double n2 = noise(static_cast<uint32_t>(i), static_cast<uint32_t>(j), 2);
            // Black as the console's backdrop draws it, with its dither, then the glow over it,
            // white, scaled by the peak and masked by the ramp, with the same noise in it.
            const double black = std::round(std::clamp(n1 + n2 - 1.0, 0.0, 1.0)) / 255.0;
            const double a = kGlowPeak * r * (1.0 + 0.25 * n1);
            const auto v = static_cast<uint8_t>(std::lround(std::clamp(black * (1.0 - a) + a, 0.0, 1.0) * 255.0));
            uint8_t* p = &pixels[(static_cast<size_t>(j) * static_cast<size_t>(w) + static_cast<size_t>(i)) * 4];
            p[0] = p[1] = p[2] = v;
            p[3] = 255;
        }
    }
    s.texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    if (!s.texture) {
        SDL_Log("2pstart: no texture for the bars around the game: %s", SDL_GetError());
        return nullptr;
    }
    SDL_UpdateTexture(s.texture, nullptr, pixels.data(), w * 4);
    SDL_SetTextureBlendMode(s.texture, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(s.texture, SDL_SCALEMODE_NEAREST);
    s.thick = thick;
    return s.texture;
}

void Glow::draw(const SDL_FRect& screen, const SDL_FRect& game) {
    // The screen and the game's edges on this window's whole pixels, the game kept on the screen
    // (LinkUpPlayScreen::drawPlace).
    const int x = static_cast<int>(std::lround(screen.x));
    const int y = static_cast<int>(std::lround(screen.y));
    const int w = static_cast<int>(std::lround(screen.x + screen.w)) - x;
    const int h = static_cast<int>(std::lround(screen.y + screen.h)) - y;
    if (w <= 0 || h <= 0 || game.w <= 0.0f || game.h <= 0.0f) return;
    const int left = std::clamp(static_cast<int>(std::lround(game.x)) - x, 0, w);
    const int right = std::clamp(static_cast<int>(std::lround(game.x + game.w)) - x, left, w);
    const int top = std::clamp(static_cast<int>(std::lround(game.y)) - y, 0, h);
    const int bottom = std::clamp(static_cast<int>(std::lround(game.y + game.h)) - y, top, h);
    // The side bars the full height, the top and bottom ones only the picture's width, so the
    // corners belong to the sides.
    const auto lay = [&](Side side, int thick, float bx, float by, float bw, float bh) {
        if (thick <= 0 || bw <= 0.0f || bh <= 0.0f) return;
        if (SDL_Texture* t = strip(side, thick)) {
            const SDL_FRect at{bx, by, bw, bh};
            SDL_RenderTextureTiled(renderer_, t, nullptr, 1.0f, &at);
        }
    };
    const auto f = [](int v) { return static_cast<float>(v); };
    lay(kLeft, left, f(x), f(y), f(left), f(h));
    lay(kRight, w - right, f(x + right), f(y), f(w - right), f(h));
    lay(kTop, top, f(x + left), f(y), f(right - left), f(top));
    lay(kBottom, h - bottom, f(x + left), f(y + bottom), f(right - left), f(h - bottom));
}

}  // namespace ui
