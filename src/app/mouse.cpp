// The mouse, in 2P Start's window (app.h): pointing focuses, and a click presses.

#include "app/app.h"

#include <algorithm>

#include "app/shared.h"

namespace {

using moves::Move;
using Then = moves::Step::Then;

}  // namespace

using namespace shared;

bool App::spotAt(float x, float y, ui::Spot& out, bool overlayOnly) const {
    float cx = 0, cy = 0;
    canvas_.toCanvas(x, y, cx, cy);
    const std::vector<ui::Spot>& spots = canvas_.spots();
    for (auto it = spots.rbegin(); it != spots.rend(); ++it) {
        if (overlayOnly && it->id < parts::spot::kOption) continue;
        if (it->contains(cx, cy)) {
            out = *it;
            return true;
        }
    }
    return false;
}

// Pointing focuses and a click presses. Over a panel only the panel counts, and a click outside it
// is Back. First run's lines take focus only from a click, so the pointer resting on one does not
// move where typing goes. A click on a line's letters puts its caret there, and a drag from it
// chooses them.
void App::pointAt(float x, float y, int clicks) {
    namespace spot = parts::spot;
    const bool click = clicks > 0;
    float cx = 0, cy = 0;
    canvas_.toCanvas(x, y, cx, cy);
    ui::Spot s;
    if (overlay_.kind != Overlay::Kind::None) {
        const bool hit = spotAt(x, y, s, true);
        const bool option = hit && s.id >= spot::kOption && s.id < spot::kField;
        if (option && (overlay_.kind == Overlay::Kind::Panel || overlay_.reading()))
            overlay_.panel.focused = s.id - spot::kOption;
        if (!click) return;
        if (overlay_.prompt) {
            move(Move::Press);   // "Click to continue": anywhere
            return;
        }
        if (!hit) move(Move::Back);
        else if (option || (overlay_.reading() && s.id == spot::kPanel)) move(Move::Press);
        else if (overlay_.kind == Overlay::Kind::Field && s.id == spot::kField) clickCaret(s.id, cx, false, clicks);
        return;
    }
    if (!spotAt(x, y, s, false)) return;
    if (click) cue(Then::Press);
    if (s.id >= spot::kBar && s.id < spot::kStatus) {
        if (!click || place_ == Place::FirstRun || typingName_) return;
        switchTo(s.id - spot::kBar == 1 ? Place::Settings : Place::Friends);
        barFocused_ = !screenTakesFocus();
        return;
    }
    switch (place_) {
        case Place::FirstRun: {
            if (!click || first_.connecting || s.id < spot::kItem || s.id > spot::kItem + 3) return;
            const int was = first_.focus;
            first_.focus = s.id - spot::kItem;
            if (first_.focus == 3) connect();
            else clickCaret(s.id, cx, first_.focus != was, clicks);
            return;
        }
        case Place::Friends:
            if (s.id == spot::kStatus) friendFocus_ = -1;
            else if (s.id >= spot::kItem && s.id < spot::kOption) friendFocus_ = s.id - spot::kItem;
            else return;
            barFocused_ = false;
            if (click) pressFriend();
            return;
        case Place::Settings: {
            // On the list, the pointer chooses: the rows beside it follow at once, as the console's do.
            if (s.id >= spot::kCategory) {
                if (typingName_ && !click) return;
                typingName_ = false;
                settingsCategory_ = s.id - spot::kCategory;
                settingsRow_ = -1;
                barFocused_ = false;
                return;
            }
            if (s.id < spot::kItem || s.id >= spot::kOption) return;
            const int row = s.id - spot::kItem;
            if (typingName_) {
                if (!click) return;
                if (row == screens::setting::Name) {
                    clickCaret(s.id, cx, false, clicks);
                    return;
                }
                typingName_ = false;
            }
            const auto& rows = screens::settingsCategories()[static_cast<size_t>(settingsCategory_)].rows;
            settingsRow_ = static_cast<int>(std::find(rows.begin(), rows.end(), row) - rows.begin());
            barFocused_ = false;
            if (!click) return;
            pressSetting(row);
            // Pressed into typing: the caret goes where the click was.
            if (typingName_) clickCaret(s.id, cx, false, clicks);
            return;
        }
        case Place::Game:
            // A click is A: on a paused host's screen, it resumes. A right-click is Back.
            if (click && session_ && session_->resumes) move(Move::Press);
            return;
    }
}
