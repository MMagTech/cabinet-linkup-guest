// The bars around the host's game and the glow over them (glow.h). Each step names the line of the
// console's kGlowFS (shaders.inc) it follows.

#include "ui/glow.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "ui/draw.h"

namespace ui {

namespace {

// The shader's still noise: a hash of the pixel's place, 0 to 1. Never animated: a moving dither in
// the corner of a dark room is a shimmer.
double noise(double x, double y, double a, double b, double k) {
    const double v = std::sin(x * a + y * b) * k;
    return v - std::floor(v);
}

// The glow's fall from the picture's edge (t = 0) to the screen's (t = 1): a flat start, so the
// light carries further before it drops.
double ramp(double t) { return 1.0 - std::pow(std::clamp(t, 0.0, 1.0), 1.7); }

}  // namespace

Glow::~Glow() {
    if (texture_) SDL_DestroyTexture(texture_);
}

void Glow::draw(const SDL_FRect& screen, const SDL_FRect& place) {
    // The screen and the game's edges on this window's whole pixels, the game kept on the screen
    // whatever the host said (LinkUpPlayScreen::setPlace and drawPlace).
    const int x = static_cast<int>(std::lround(screen.x));
    const int y = static_cast<int>(std::lround(screen.y));
    const int w = static_cast<int>(std::lround(screen.x + screen.w)) - x;
    const int h = static_cast<int>(std::lround(screen.y + screen.h)) - y;
    if (w <= 0 || h <= 0) return;
    const float px = std::clamp(place.x, 0.0f, kCanvasWidth);
    const float py = std::clamp(place.y, 0.0f, kCanvasHeight);
    const float pw = std::clamp(place.w, 0.0f, kCanvasWidth - px);
    const float ph = std::clamp(place.h, 0.0f, kCanvasHeight - py);
    if (pw <= 0.0f || ph <= 0.0f) return;
    const double kx = static_cast<double>(w) / kCanvasWidth, ky = static_cast<double>(h) / kCanvasHeight;
    const int left = std::clamp(static_cast<int>(std::lround(px * kx)), 0, w);
    const int right = std::clamp(static_cast<int>(std::lround((px + pw) * kx)), left, w);
    const int top = std::clamp(static_cast<int>(std::lround(py * ky)), 0, h);
    const int bottom = std::clamp(static_cast<int>(std::lround((py + ph) * ky)), top, h);

    const bool same = texture_ && x == x_ && y == y_ && w == w_ && h == h_ && left == left_ && right == right_ &&
                      top == top_ && bottom == bottom_;
    if (!same) {
        if (texture_) SDL_DestroyTexture(texture_);
        texture_ = nullptr;
        // t across each bar: the side bars the full height, the top and bottom ones only the
        // picture's width, so the corners belong to the sides.
        std::vector<double> across(static_cast<size_t>(w), -1.0), down(static_cast<size_t>(h), -1.0);
        for (int i = 0; i < w; ++i) {
            const double c = i + 0.5;
            if (i < left) across[static_cast<size_t>(i)] = ramp(left > 0 ? (left - c) / left : 1.0);
            else if (i >= right) across[static_cast<size_t>(i)] = ramp(w > right ? (c - right) / (w - right) : 1.0);
        }
        for (int j = 0; j < h; ++j) {
            const double c = j + 0.5;
            if (j < top) down[static_cast<size_t>(j)] = ramp(top > 0 ? (top - c) / top : 1.0);
            else if (j >= bottom) down[static_cast<size_t>(j)] = ramp(h > bottom ? (c - bottom) / (h - bottom) : 1.0);
        }
        std::vector<uint8_t> pixels(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 0);
        for (int j = 0; j < h; ++j) {
            for (int i = 0; i < w; ++i) {
                const double r = across[static_cast<size_t>(i)] >= 0.0 ? across[static_cast<size_t>(i)] : down[static_cast<size_t>(j)];
                if (r < 0.0) continue;   // inside the picture: never cover a game pixel
                const double qx = x + i, qy = y + j;
                const double n1 = noise(qx, qy, 12.9898, 78.233, 43758.5453);
                const double n2 = noise(qx, qy, 39.3468, 11.1353, 24634.6345);
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
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
        if (!texture_) {
            SDL_Log("2pstart: no texture for the bars around the game: %s", SDL_GetError());
            return;
        }
        SDL_UpdateTexture(texture_, nullptr, pixels.data(), w * 4);
        SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
        x_ = x, y_ = y, w_ = w, h_ = h;
        left_ = left, right_ = right, top_ = top, bottom_ = bottom;
    }
    const SDL_FRect at{static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)};
    SDL_RenderTexture(renderer_, texture_, nullptr, &at);
}

}  // namespace ui
