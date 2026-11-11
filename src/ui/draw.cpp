// What every screen is drawn with (draw.h).

#include "ui/draw.h"

#include <algorithm>
#include <cmath>

namespace ui {

namespace {

float styleSize(TextStyle s) {
    switch (s) {
        case TextStyle::LargeTitle: return 76;
        case TextStyle::Title2: return 48;
        case TextStyle::Title3: return 38;
        case TextStyle::Callout: return 31;
        case TextStyle::Body: return 29;
        case TextStyle::Footnote: return 29;
        case TextStyle::Caption1: return 25;
    }
    return 29;
}

// 0 regular, 1 semibold, 2 bold.
int styleWeight(TextStyle s) {
    switch (s) {
        case TextStyle::LargeTitle:
        case TextStyle::Title2: return 2;
        case TextStyle::Title3: return 1;
        default: return 0;
    }
}

const char* kFontFiles[] = {"NotoSans-Regular.ttf", "NotoSans-SemiBold.ttf", "NotoSans-Bold.ttf"};

Color mix(const Color& a, const Color& b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
}

SDL_FColor shade(const Color& c, float alpha) { return {c.r, c.g, c.b, c.a * alpha}; }

// A rounded rectangle's edge in pixels, clockwise, `steps` pieces a corner. Two edges made with
// the same `steps` have their points side by side, so a band can be laid between them.
void edge(std::vector<SDL_FPoint>& out, float x, float y, float w, float h, float r, int steps) {
    out.clear();
    w = std::max(w, 0.0f);
    h = std::max(h, 0.0f);
    r = std::clamp(r, 0.0f, std::min(w, h) * 0.5f);
    const float cx[4] = {x + w - r, x + w - r, x + r, x + r};
    const float cy[4] = {y + r, y + h - r, y + h - r, y + r};
    const float from[4] = {-90.0f, 0.0f, 90.0f, 180.0f};
    for (int corner = 0; corner < 4; ++corner) {
        for (int i = 0; i <= steps; ++i) {
            const float angle = (from[corner] + 90.0f * static_cast<float>(i) / static_cast<float>(steps)) * 0.01745329252f;
            out.push_back({cx[corner] + std::cos(angle) * r, cy[corner] + std::sin(angle) * r});
        }
    }
}

struct Mesh {
    std::vector<SDL_Vertex> vertices;
    std::vector<int> indices;
    int add(SDL_FPoint p, SDL_FColor c) {
        vertices.push_back({p, c, {0, 0}});
        return static_cast<int>(vertices.size()) - 1;
    }
    // A band between two edges of the same length, each with its own colours.
    template <typename ColourA, typename ColourB>
    void band(const std::vector<SDL_FPoint>& a, ColourA&& ca, const std::vector<SDL_FPoint>& b, ColourB&& cb) {
        const int n = static_cast<int>(a.size());
        const int first = static_cast<int>(vertices.size());
        for (int i = 0; i < n; ++i) {
            add(a[static_cast<size_t>(i)], ca(a[static_cast<size_t>(i)]));
            add(b[static_cast<size_t>(i)], cb(a[static_cast<size_t>(i)]));
        }
        for (int i = 0; i < n; ++i) {
            const int j = (i + 1) % n;
            const int a0 = first + i * 2, b0 = a0 + 1, a1 = first + j * 2, b1 = a1 + 1;
            indices.insert(indices.end(), {a0, b0, b1, a0, b1, a1});
        }
    }
    // Everything inside one edge, from a point in the middle.
    template <typename Colour>
    void inside(const std::vector<SDL_FPoint>& a, SDL_FPoint middle, Colour&& c) {
        const int n = static_cast<int>(a.size());
        const int centre = add(middle, c(middle));
        const int first = static_cast<int>(vertices.size());
        for (const SDL_FPoint& p : a) add(p, c(p));
        for (int i = 0; i < n; ++i) indices.insert(indices.end(), {centre, first + i, first + (i + 1) % n});
    }
    void draw(SDL_Renderer* r) {
        if (!indices.empty())
            SDL_RenderGeometry(r, nullptr, vertices.data(), static_cast<int>(vertices.size()), indices.data(),
                               static_cast<int>(indices.size()));
    }
};

}  // namespace

Canvas::Canvas(SDL_Renderer* renderer, const std::string& fonts) : renderer_(renderer), fonts_(fonts) {
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    ready_ = TTF_Init();
    setOutput(static_cast<int>(kCanvasWidth), static_cast<int>(kCanvasHeight));
    ready_ = ready_ && font(TextStyle::Body) && font(TextStyle::Title3) && font(TextStyle::Title2);
}

Canvas::~Canvas() {
    forget();
    TTF_Quit();
}

void Canvas::forget() {
    for (auto& [key, w] : texts_)
        if (w.texture) SDL_DestroyTexture(w.texture);
    texts_.clear();
    for (auto& [key, f] : opened_)
        if (f) TTF_CloseFont(f);
    opened_.clear();
}

void Canvas::setOutput(int width, int height) {
    if (width == outW_ && height == outH_) return;
    outW_ = width;
    outH_ = height;
    const float scale = std::min(static_cast<float>(width) / kCanvasWidth, static_cast<float>(height) / kCanvasHeight);
    // Words are made at the size they are shown, so a new size means making them again.
    if (scale != scale_) forget();
    scale_ = scale;
    ox_ = (static_cast<float>(width) - kCanvasWidth * scale_) * 0.5f;
    oy_ = (static_cast<float>(height) - kCanvasHeight * scale_) * 0.5f;
}

void Canvas::setClip(float x, float y, float w, float h) {
    const SDL_Rect clip{static_cast<int>(std::floor(ox_ + px(x))), static_cast<int>(std::floor(oy_ + px(y))),
                        static_cast<int>(std::ceil(px(w))), static_cast<int>(std::ceil(px(h)))};
    SDL_SetRenderClipRect(renderer_, &clip);
}

void Canvas::clearClip() { SDL_SetRenderClipRect(renderer_, nullptr); }

void Canvas::backdrop(float alpha) {
    const float w = static_cast<float>(outW_), h = static_cast<float>(outH_);
    const float mid = oy_ + px(kCanvasHeight * 0.55f);
    const SDL_FColor top = shade(look_.top, alpha), middle = shade(look_.mid, alpha), bottom = shade(look_.bottom, alpha);
    const SDL_Vertex v[6] = {{{0, 0}, top, {0, 0}},      {{w, 0}, top, {0, 0}},      {{0, mid}, middle, {0, 0}},
                             {{w, mid}, middle, {0, 0}}, {{0, h}, bottom, {0, 0}}, {{w, h}, bottom, {0, 0}}};
    const int at[12] = {0, 1, 3, 0, 3, 2, 2, 3, 5, 2, 5, 4};
    SDL_RenderGeometry(renderer_, nullptr, v, 6, at, 12);
}

void Canvas::draw(const Rect& r) {
    // Edges sit on whole pixels, so a straight side is sharp and its one-pixel fade is even.
    const float x = std::round(ox_ + px(r.x)), y = std::round(oy_ + px(r.y));
    const float w = std::round(ox_ + px(r.x + r.w)) - x, h = std::round(oy_ + px(r.y + r.h)) - y;
    const float radius = std::min(px(r.radius), std::min(w, h) * 0.5f);
    const int steps = std::clamp(static_cast<int>(radius * 0.5f) + 2, 2, 40);
    std::vector<SDL_FPoint> a, b;
    // Grown or shrunk by `d` pixels all round, its corners with it.
    const auto grown = [&](std::vector<SDL_FPoint>& out, float d, float dy) {
        edge(out, x - d, y - d + dy, w + d * 2.0f, h + d * 2.0f, std::max(radius + d, 0.0f), steps);
    };
    Mesh mesh;

    if (r.shadowBlur > 0.0f && r.shadowColor.a > 0.0f) {
        // Outside the shape only, so a see-through shape is not darkened by its own shadow: from
        // the shape's edge, fading in three bands to nothing at the blur's reach, and sitting lower
        // the further out it goes.
        const float blur = px(r.shadowBlur), down = px(r.shadowOffsetY);
        const float at[4] = {0.0f, 0.3f, 0.6f, 1.0f};
        const float strength[4] = {0.75f, 0.33f, 0.09f, 0.0f};
        const auto level = [&](int i) {
            const SDL_FColor c = shade(r.shadowColor, strength[i] * alpha_);
            return [c](SDL_FPoint) { return c; };
        };
        for (int i = 0; i < 3; ++i) {
            grown(a, at[i] * blur, at[i] * down);
            grown(b, at[i + 1] * blur, at[i + 1] * down);
            mesh.band(a, level(i), b, level(i + 1));
        }
    }

    if (r.fill.a > 0.0f || (r.gradient && r.fillBottom.a > 0.0f)) {
        const auto colour = [&](float alpha) {
            return [&, alpha](SDL_FPoint p) {
                if (!r.gradient) return shade(r.fill, alpha * alpha_);
                const float t = r.across ? (w > 0 ? (p.x - x) / w : 0.0f) : (h > 0 ? (p.y - y) / h : 0.0f);
                return shade(mix(r.fill, r.fillBottom, std::clamp(t, 0.0f, 1.0f)), alpha * alpha_);
            };
        };
        // Solid to half a pixel inside the edge, then faded out over one pixel: a smooth edge.
        grown(a, -0.5f, 0.0f);
        grown(b, 0.5f, 0.0f);
        mesh.inside(a, {x + w * 0.5f, y + h * 0.5f}, colour(1.0f));
        mesh.band(a, colour(1.0f), b, colour(0.0f));
    }

    if (r.border > 0.0f && r.borderColor.a > 0.0f) {
        const float width = std::max(std::round(px(r.border)), 1.0f);
        const SDL_FColor on = shade(r.borderColor, alpha_), off = shade(r.borderColor, 0.0f);
        const auto solid = [on](SDL_FPoint) { return on; };
        const auto clear = [off](SDL_FPoint) { return off; };
        std::vector<SDL_FPoint> outer, inner;
        grown(outer, 0.5f, 0.0f);
        grown(a, -0.5f, 0.0f);
        grown(b, -width, 0.0f);
        grown(inner, -width - 1.0f, 0.0f);
        mesh.band(outer, clear, a, solid);
        mesh.band(a, solid, b, solid);
        mesh.band(b, solid, inner, clear);
    }
    mesh.draw(renderer_);
}

TTF_Font* Canvas::font(TextStyle st) {
    const int weight = styleWeight(st);
    const int size = std::max(6, static_cast<int>(std::lround(styleSize(st) * scale_)));
    const auto key = std::make_pair(weight, size);
    const auto found = opened_.find(key);
    if (found != opened_.end()) return found->second;
    TTF_Font* f = TTF_OpenFont((fonts_ + "/" + kFontFiles[weight]).c_str(), static_cast<float>(size));
    if (!f) SDL_Log("no font: %s", SDL_GetError());
    opened_[key] = f;
    return f;
}

float Canvas::measure(const std::string& s, TextStyle st) {
    TTF_Font* f = font(st);
    int w = 0, h = 0;
    if (!f || s.empty() || !TTF_GetStringSize(f, s.data(), s.size(), &w, &h)) return 0.0f;
    return static_cast<float>(w) / scale_;
}

float Canvas::lineHeight(TextStyle st) {
    TTF_Font* f = font(st);
    return f ? static_cast<float>(TTF_GetFontHeight(f)) / scale_ : styleSize(st) * 1.36f;
}

float Canvas::ascent(TextStyle st) {
    TTF_Font* f = font(st);
    return f ? static_cast<float>(TTF_GetFontAscent(f)) / scale_ : styleSize(st) * 1.07f;
}

// Noto Sans's capitals stand 714 of its 1000 units tall.
float Canvas::capHeight(TextStyle st) { return styleSize(st) * 0.714f; }

std::string Canvas::truncate(const std::string& s, TextStyle st, float width) {
    if (measure(s, st) <= width) return s;
    static const std::string kMore = "\xE2\x80\xA6";
    std::string kept = s;
    while (!kept.empty()) {
        // Back one whole character, never into the middle of one.
        size_t cut = kept.size() - 1;
        while (cut > 0 && (static_cast<unsigned char>(kept[cut]) & 0xC0) == 0x80) --cut;
        kept.erase(cut);
        while (!kept.empty() && kept.back() == ' ') kept.pop_back();
        if (measure(kept + kMore, st) <= width) return kept + kMore;
    }
    return kMore;
}

const Canvas::Written& Canvas::written(const std::string& s, TextStyle st) {
    const std::string key = std::to_string(styleWeight(st)) + ":" + std::to_string(static_cast<int>(styleSize(st))) + ":" + s;
    const auto found = texts_.find(key);
    if (found != texts_.end()) return found->second;
    Written w;
    if (TTF_Font* f = font(st)) {
        if (SDL_Surface* surface = TTF_RenderText_Blended(f, s.data(), s.size(), SDL_Color{255, 255, 255, 255})) {
            w.texture = SDL_CreateTextureFromSurface(renderer_, surface);
            w.w = static_cast<float>(surface->w);
            w.h = static_cast<float>(surface->h);
            SDL_DestroySurface(surface);
        }
    }
    return texts_[key] = w;
}

void Canvas::text(const std::string& s, float x, float baseline, TextStyle st, Color c) {
    if (s.empty() || c.a * alpha_ <= 0.0f) return;
    const Written& w = written(s, st);
    TTF_Font* f = font(st);
    if (!w.texture || !f) return;
    SDL_SetTextureColorModFloat(w.texture, c.r, c.g, c.b);
    SDL_SetTextureAlphaModFloat(w.texture, c.a * alpha_);
    const SDL_FRect to{std::round(ox_ + px(x)), std::round(oy_ + px(baseline) - static_cast<float>(TTF_GetFontAscent(f))), w.w, w.h};
    SDL_RenderTexture(renderer_, w.texture, nullptr, &to);
}

void Canvas::centred(const std::string& s, float midX, float baseline, TextStyle st, Color c) {
    text(s, midX - measure(s, st) * 0.5f, baseline, st, c);
}

}  // namespace ui
