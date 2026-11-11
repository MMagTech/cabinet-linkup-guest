// The parts more than one screen draws (parts.h).

#include "ui/parts.h"

#include <algorithm>
#include <cmath>

namespace parts {

using ui::Color;
using ui::Rect;
using ui::TextStyle;

namespace {

constexpr float kPillPadX = 14.0f;
constexpr float kPillPadY = 8.0f;
constexpr float kPillFocusScale = 1.06f;
constexpr float kFocusedTint = 0.55f;
constexpr float kSelectedTint = 0.30f;

// The panel a menu sits on (panels.h, menuPanel).
Rect menuPanel(Color fill, float x, float y, float w, float h) {
    fill.a = 0.92f;
    Rect p{x, y, w, h, 32.0f, fill};
    p.gradient = true;
    p.fillBottom = Color{fill.r * 0.62f, fill.g * 0.62f, fill.b * 0.62f, 0.96f};
    p.border = 1.5f;
    p.borderColor = Color::white(0.14f);
    p.shadowBlur = 64.0f;
    p.shadowOffsetY = 22.0f;
    p.shadowColor = Color::black(0.60f);
    return p;
}

// A button on such a panel, at rest and under focus (panels.h, menuButton).
Rect menuButton(float x, float y, float w, float h, float radius, float f) {
    Rect b{x, y, w, h, radius, Color::white(0.06f + f * 0.10f)};
    if (f > 0.0f) {
        b.border = f * kFocusRim;
        b.borderColor = ui::palette::kFocusRim;
        b.shadowBlur = 22.0f;
        b.shadowOffsetY = 6.0f;
        b.shadowColor = Color::black(0.45f * f);
    }
    return b;
}

Color menuLabel(float f) { return Color::white(0.62f + f * 0.38f); }

}  // namespace

void topBar(ui::Canvas& c, const std::vector<std::string>& labels, int selected, float focus) {
    constexpr float kItemGap = 60.0f, kPadX = 22.0f, kInsetY = 6.0f;
    const TextStyle st = TextStyle::Callout;
    const float baseline = kBarTop + (kBarHeight - c.lineHeight(st)) * 0.5f + c.ascent(st);
    float x = kInset;
    for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
        const std::string& label = labels[static_cast<size_t>(i)];
        const float w = c.measure(label, st);
        const bool on = i == selected;
        if (on) {
            const float h = kBarHeight - kInsetY * 2.0f;
            const float tint = kSelectedTint + focus * (kFocusedTint - kSelectedTint);
            c.draw(Rect{x - kPadX, kBarTop + kInsetY, w + kPadX * 2.0f, h, h * 0.5f, Color::white(tint * 0.36f)});
        }
        c.text(label, x, baseline, st, Color::white(on ? 1.0f : 0.65f));
        x += w + kItemGap;
    }
}

float pillHeight(ui::Canvas& c) { return c.lineHeight(TextStyle::Title3) + kPillPadY * 2.0f; }

void pill(ui::Canvas& c, const std::string& label, float x, float y, bool selected, float focus) {
    const TextStyle st = TextStyle::Title3;
    const float w0 = c.measure(label, st) + kPillPadX * 2.0f;
    const float h0 = pillHeight(c);
    const float s = 1.0f + focus * (kPillFocusScale - 1.0f);
    const float w = w0 * s, h = h0 * s;
    // `x` is where the words start. The capsule grows round them and does not push them along.
    const float px = x - kPillPadX - (w - w0) * 0.5f;
    const float py = y - (h - h0) * 0.5f;
    if (focus > 0.001f) c.draw(Rect{px, py, w, h, h * 0.5f, Color::white(focus * kFocusedTint * 0.5f)});
    c.text(label, x, py + kPillPadY * s + c.ascent(st), st, Color::white(selected ? 1.0f : 0.45f));
}

void pressPrompt(ui::Canvas& c, const std::string& before, const std::string& button, const std::string& after,
                 float midX, float baseline, float alpha) {
    const TextStyle st = TextStyle::Title3;
    const float gap = 16.0f;
    const float badge = c.lineHeight(st) * 0.95f;
    const float w1 = c.measure(before, st), w2 = c.measure(after, st);
    float x = midX - (w1 + gap + badge + gap + w2) * 0.5f;
    c.text(before, x, baseline, st, Color::white(0.92f * alpha));
    x += w1 + gap;
    const float capMid = baseline - c.ascent(st) * 0.36f;
    c.draw(Rect{x, capMid - badge * 0.5f, badge, badge, badge * 0.5f, Color::white(0.95f * alpha)});
    Color ink = ui::palette::kInk;
    ink.a = alpha;
    c.text(button, x + (badge - c.measure(button, st)) * 0.5f, capMid + c.ascent(st) * 0.36f, st, ink);
    x += badge + gap;
    c.text(after, x, baseline, st, Color::white(0.92f * alpha));
}

void notice(ui::Canvas& c, const std::string& words, Tone tone) {
    constexpr float kPillH = 64.0f, kPad = 30.0f, kDot = 14.0f, kGap = 16.0f;
    const TextStyle st = TextStyle::Callout;
    const float w = kPad + kDot + kGap + c.measure(words, st) + kPad;
    const float x = (ui::kCanvasWidth - w) * 0.5f;
    const float y = ui::kCanvasHeight - ui::kSafeInset - kPillH - 24.0f;
    Color fill = c.surface();
    fill.a = 0.96f;
    Rect body{x, y, w, kPillH, kPillH * 0.5f, fill};
    body.border = 1.5f;
    body.borderColor = Color::white(0.14f);
    body.shadowBlur = 26.0f;
    body.shadowOffsetY = 10.0f;
    body.shadowColor = Color::black(0.45f);
    c.draw(body);
    const Color dot = tone == Tone::Done ? ui::palette::kScreenCyan
                      : tone == Tone::Problem ? ui::palette::kMarqueeAmber
                                              : Color::white(0.70f);
    c.draw(Rect{x + kPad, y + (kPillH - kDot) * 0.5f, kDot, kDot, kDot * 0.5f, dot});
    c.text(words, x + kPad + kDot + kGap, y + (kPillH - c.lineHeight(st)) * 0.5f + c.ascent(st), st, Color::white(0.94f));
}

void panel(ui::Canvas& c, const Panel& p) {
    constexpr float kPad = 40.0f, kMaxW = 1200.0f, kButtonW = 600.0f, kButtonH = 88.0f, kButtonGap = 12.0f,
                    kButtonRadius = 16.0f, kFocusScale = 1.04f;
    const float W = ui::kCanvasWidth, H = ui::kCanvasHeight;
    const bool prompt = p.options.empty() && !p.promptButton.empty();
    float inner = std::max(kButtonW, c.measure(p.title, TextStyle::Title2));
    if (!p.detail.empty()) inner = std::max(inner, c.measure(p.detail, TextStyle::Callout));
    const float panelW = std::min(kMaxW, inner + kPad * 2.0f);
    const float textMax = panelW - kPad * 2.0f;
    const float titleH = c.lineHeight(TextStyle::Title2);
    const float detailH = p.detail.empty() ? 0.0f : 8.0f + c.lineHeight(TextStyle::Callout);
    const int n = static_cast<int>(p.options.size());
    const float listH = prompt ? kButtonH : n == 0 ? 0.0f : static_cast<float>(n) * kButtonH + static_cast<float>(n - 1) * kButtonGap;
    const float below = listH > 0.0f ? 36.0f + listH : 0.0f;
    const float panelH = kPad + titleH + detailH + below + kPad;
    const float px = (W - panelW) * 0.5f, py = (H - panelH) * 0.5f;

    c.draw(Rect{0, 0, W, H, 0, Color::black(0.55f)});
    c.draw(menuPanel(c.surface(), px, py, panelW, panelH));

    float y = py + kPad;
    c.centred(c.truncate(p.title, TextStyle::Title2, textMax), W * 0.5f, y + c.ascent(TextStyle::Title2), TextStyle::Title2,
              Color::white(1.0f));
    y += titleH;
    if (!p.detail.empty())
        c.centred(c.truncate(p.detail, TextStyle::Callout, textMax), W * 0.5f, y + 8.0f + c.ascent(TextStyle::Callout),
                  TextStyle::Callout, Color::white(0.60f));
    y += detailH + 36.0f;

    if (prompt) {
        pressPrompt(c, p.promptBefore, p.promptButton, p.promptAfter, W * 0.5f,
                    y + kButtonH * 0.5f + c.ascent(TextStyle::Title3) * 0.40f, 1.0f);
        return;
    }
    const TextStyle st = TextStyle::Title3;
    // The focused one last, so it grows over its neighbours.
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < n; ++i) {
            const bool on = i == p.focused;
            if ((pass == 1) != on) continue;
            const float f = on ? 1.0f : 0.0f;
            const float s = 1.0f + f * (kFocusScale - 1.0f);
            const float bw = kButtonW * s, bh = kButtonH * s;
            const float bx = (W - bw) * 0.5f;
            const float by = y + static_cast<float>(i) * (kButtonH + kButtonGap) - (bh - kButtonH) * 0.5f;
            c.draw(menuButton(bx, by, bw, bh, kButtonRadius, f));
            c.centred(p.options[static_cast<size_t>(i)], W * 0.5f, by + (bh - c.lineHeight(st)) * 0.5f + c.ascent(st), st,
                      menuLabel(f));
        }
    }
}

void row(ui::Canvas& c, float x, float y, float w, const std::string& label, const std::string& value, float focus,
         bool dim) {
    const float s = 1.0f + focus * 0.02f;
    const float bw = w * s, bh = kRowHeight * s;
    const float bx = x - (bw - w) * 0.5f, by = y - (bh - kRowHeight) * 0.5f;
    c.draw(menuButton(bx, by, bw, bh, 18.0f, focus));
    const TextStyle st = TextStyle::Title3, vs = TextStyle::Callout;
    constexpr float kPad = 32.0f;
    c.text(label, x + kPad, y + (kRowHeight - c.lineHeight(st)) * 0.5f + c.ascent(st), st, menuLabel(focus));
    const float room = w - kPad * 3.0f - c.measure(label, st);
    const std::string shown = c.truncate(value, vs, room);
    c.text(shown, x + w - kPad - c.measure(shown, vs), y + (kRowHeight - c.lineHeight(vs)) * 0.5f + c.ascent(vs), vs,
           Color::white(dim ? 0.35f : 0.60f + focus * 0.34f));
}

void button(ui::Canvas& c, float x, float y, float w, const std::string& label, float focus) {
    const float s = 1.0f + focus * 0.02f;
    const float bw = w * s, bh = kRowHeight * s;
    c.draw(menuButton(x - (bw - w) * 0.5f, y - (bh - kRowHeight) * 0.5f, bw, bh, 18.0f, focus));
    const TextStyle st = TextStyle::Title3;
    c.centred(label, x + w * 0.5f, y + (kRowHeight - c.lineHeight(st)) * 0.5f + c.ascent(st), st, menuLabel(focus));
}

void cabinet(ui::Canvas& c, float x, float y, float h) {
    // The cabinet spans x 248 to 775 and y 130 to 850 of its own 1024 space (startscreen.cpp).
    const float s = h / 720.0f;
    const float ox = x - 248.0f * s, oy = y - 130.0f * s;
    const auto R = [&](float rx, float ry, float rw, float rh, float rad, Color col) {
        c.draw(Rect{ox + rx * s, oy + ry * s, rw * s, rh * s, rad * s, col});
    };
    const auto dot = [&](float cx, float cy, float rad, Color col) { R(cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, rad, col); };
    const Color body = Color::rgb(0xF2EEE8), deck = Color::rgb(0xD7D1C8);

    R(268, 130, 488, 670, 42, body);
    // The marquee, pink to amber, left to right.
    Rect marquee{ox + 318 * s, oy + 176 * s, 390 * s, 84 * s, 16 * s, Color::rgb(0xFF96CA)};
    marquee.gradient = true;
    marquee.across = true;
    marquee.fillBottom = Color::rgb(0xFFCB6E);
    c.draw(marquee);
    // The screen, amber down to red.
    Rect screen{ox + 316 * s, oy + 292 * s, 394 * s, 284 * s, 18 * s, Color::rgb(0xFEE08F)};
    screen.gradient = true;
    screen.fillBottom = Color::rgb(0xEB5736);
    c.draw(screen);

    R(248, 600, 527, 112, 20, deck);
    dot(348, 656, 30, Color::rgb(0x3A3444));
    dot(500, 656, 25, Color::rgb(0xEC405C));
    dot(583, 656, 24, Color::rgb(0xFFC457));
    dot(692, 656, 54, Color::rgb(0xFFB27A, 0.10f));
    dot(692, 656, 40, Color::rgb(0xFFB27A, 0.16f));
    dot(692, 656, 28, Color::rgb(0xFFB27A, 0.22f));
    R(668, 646, 48, 20, 10, Color::rgb(0xFFF0E2));
    R(303, 795, 419, 55, 14, deck);
}

void cabinetDim(ui::Canvas& c, float x, float y, float h) {
    // It is stacked shapes, and made see-through they show through each other. So it is drawn
    // whole and the background comes over it, only there.
    cabinet(c, x, y, h);
    constexpr float kMargin = 24.0f;
    const float w = h * 527.0f / 720.0f;
    c.setClip(x - kMargin, y - kMargin, w + kMargin * 2.0f, h + kMargin * 2.0f);
    c.backdrop(0.68f);
    c.clearClip();
}

void waitingDots(ui::Canvas& c, float midX, float y, float clock) {
    constexpr float kDot = 16.0f, kDotGap = 22.0f;
    const float x0 = midX - (kDot * 3.0f + kDotGap * 2.0f) * 0.5f;
    for (int i = 0; i < 3; ++i) {
        const float phase = clock * 3.0f - static_cast<float>(i) * 0.7f;
        const float alpha = 0.25f + 0.65f * (0.5f + 0.5f * std::sin(phase));
        c.draw(Rect{x0 + static_cast<float>(i) * (kDot + kDotGap), y - kDot, kDot, kDot, kDot * 0.5f, Color::white(alpha)});
    }
}

}  // namespace parts
