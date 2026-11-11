// What every screen is drawn with: rounded rectangles and words, on the console's own canvas.
//
// THE CANVAS IS THE CONSOLE'S. CabinetOS lays every screen out on 1920 by 1080 and its sizes, colours
// and type sizes are copied here from its frontend (ui.h, text.cpp, design.h), so a screen drawn
// with the same numbers comes out the same. The canvas is fitted into whatever window there is.
//
// The console draws with its own OpenGL code. This draws with SDL's renderer, which runs on
// Direct3D, Metal or plain memory alike, so the same code makes the app's window and the pictures
// the gallery saves.

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

namespace ui {

constexpr float kCanvasWidth = 1920.0f;
constexpr float kCanvasHeight = 1080.0f;
constexpr float kSafeInset = 60.0f;

struct Color {
    float r = 1, g = 1, b = 1, a = 1;
    static constexpr Color rgb(uint32_t hex, float alpha = 1.0f) {
        return {static_cast<float>((hex >> 16) & 0xFF) / 255.0f, static_cast<float>((hex >> 8) & 0xFF) / 255.0f,
                static_cast<float>(hex & 0xFF) / 255.0f, alpha};
    }
    static constexpr Color white(float alpha) { return {1, 1, 1, alpha}; }
    static constexpr Color black(float alpha) { return {0, 0, 0, alpha}; }
};

// The two accents, the focus rim, and the dark of a letter on a white button badge.
namespace palette {
constexpr Color kScreenCyan = Color::rgb(0x58E8F6);
constexpr Color kMarqueeAmber = Color::rgb(0xFFC457);
constexpr Color kFocusRim = Color::white(0.85f);
constexpr Color kInk = Color{0.07f, 0.05f, 0.12f, 1.0f};
}  // namespace palette

// What the menus are drawn in: the background's three stops, the middle one 0.55 of the way down,
// and the colour of a panel. look.h makes these from the person's choice; purple until it does.
struct Look {
    Color top = Color::rgb(0x3A2268);
    Color mid = Color::rgb(0x120C26);
    Color bottom = Color::rgb(0x090614);
    Color surface = Color::rgb(0x241A3D);
};

// One rounded rectangle. Everything on a screen is one of these, or a few stacked.
struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    float radius = 0;
    Color fill = Color::white(0);
    // A gradient from `fill` to `fillBottom`: top to bottom, or left to right with `across`.
    bool gradient = false;
    bool across = false;
    Color fillBottom = Color::white(0);
    // Drawn inside the edge, so a focused shape does not grow by its rim.
    float border = 0;
    Color borderColor = Color::white(0);
    // A soft shadow behind the shape: how far it fades, and how far down it sits.
    float shadowBlur = 0;
    float shadowOffsetY = 0;
    Color shadowColor = Color::black(0);
};

// The console's type sizes, in canvas points (text.cpp).
enum class TextStyle { LargeTitle, Title2, Title3, Callout, Body, Footnote, Caption1 };

class Canvas {
public:
    // `fonts` is the folder that holds Noto Sans in its three weights.
    Canvas(SDL_Renderer* renderer, const std::string& fonts);
    ~Canvas();
    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;

    // False when a font could not be opened: nothing can be written.
    bool ready() const { return ready_; }

    // Fits the canvas into this many pixels, centred, with bars where the shape differs.
    void setOutput(int width, int height);

    // Everything drawn after this is this much there, 0 to 1: a screen fading as one.
    void setAlpha(float alpha) { alpha_ = alpha; }
    // Drawing kept to this part of the canvas, until cleared.
    void setClip(float x, float y, float w, float h);
    void clearClip();

    void setLook(const Look& look) { look_ = look; }
    Color surface() const { return look_.surface; }

    // The menus' background, over the whole window. `alpha` under 1 lays it over what is there.
    void backdrop(float alpha = 1.0f);
    void draw(const Rect& r);

    float measure(const std::string& s, TextStyle st);
    float lineHeight(TextStyle st);
    float ascent(TextStyle st);
    float capHeight(TextStyle st);
    // `s`, or as much of it as fits `width` with an ellipsis.
    std::string truncate(const std::string& s, TextStyle st, float width);
    void text(const std::string& s, float x, float baseline, TextStyle st, Color c);
    void centred(const std::string& s, float midX, float baseline, TextStyle st, Color c);

private:
    struct Written {
        SDL_Texture* texture = nullptr;
        float w = 0, h = 0;
    };
    TTF_Font* font(TextStyle st);
    const Written& written(const std::string& s, TextStyle st);
    void forget();
    float px(float canvas) const { return canvas * scale_; }

    SDL_Renderer* renderer_;
    std::string fonts_;
    bool ready_ = false;
    int outW_ = 0, outH_ = 0;
    float scale_ = 1.0f, ox_ = 0.0f, oy_ = 0.0f;
    float alpha_ = 1.0f;
    Look look_;
    std::map<std::pair<int, int>, TTF_Font*> opened_;     // by weight and pixel size
    std::map<std::string, Written> texts_;
};

}  // namespace ui
