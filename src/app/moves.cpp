// Where focus goes (moves.h).

#include "app/moves.h"

#include <algorithm>

namespace moves {

Step friends(int focus, int tiles, Move m) {
    using Then = Step::Then;
    auto go = [](int to) { return Step{to, Then::Moved}; };
    const Step edge{focus, Then::Edge};
    if (m == Move::Back) return {focus, Then::Back};
    if (tiles <= 0) return m == Move::Up ? Step{focus, Then::ToBar} : edge;
    if (focus < 0) {
        if (m == Move::Up) return {focus, Then::ToBar};
        if (m == Move::Press) return {focus, Then::Press};
        if (m == Move::Down) return go(0);
        return edge;
    }
    focus = std::min(focus, tiles - 1);
    switch (m) {
        case Move::Up: return go(focus < kColumns ? -1 : focus - kColumns);
        case Move::Down:
            if (focus / kColumns == (tiles - 1) / kColumns) return edge;
            return go(std::min(focus + kColumns, tiles - 1));
        case Move::Left:
            if (focus % kColumns == 0) return edge;
            return go(focus - 1);
        case Move::Right:
            if (focus % kColumns == kColumns - 1 || focus + 1 >= tiles) return edge;
            return go(focus + 1);
        case Move::Press: return {focus, Then::Press};
        default: return edge;
    }
}

Step column(int focus, int count, Move m) {
    using Then = Step::Then;
    const Step edge{focus, Then::Edge};
    if (m == Move::Back) return {focus, Then::Back};
    if (count <= 0) return edge;
    focus = std::clamp(focus, 0, count - 1);
    switch (m) {
        case Move::Up:
            if (focus > 0) return {focus - 1, Then::Moved};
            return edge;
        case Move::Down:
            if (focus + 1 < count) return {focus + 1, Then::Moved};
            return edge;
        case Move::Press: return {focus, Then::Press};
        default: return edge;
    }
}

SettingsStep settings(int category, int row, const std::vector<std::vector<bool>>& focusable, Move m) {
    using Then = Step::Then;
    const int cats = static_cast<int>(focusable.size());
    SettingsStep s{std::clamp(category, 0, std::max(0, cats - 1)), row, Then::Edge};
    if (cats == 0) {
        s.then = m == Move::Up ? Then::ToBar : m == Move::Back ? Then::Back : Then::Edge;
        return s;
    }
    const std::vector<bool>& rows = focusable[static_cast<size_t>(s.category)];
    const int count = static_cast<int>(rows.size());
    const auto takes = [&](int r) { return r >= 0 && r < count && rows[static_cast<size_t>(r)]; };
    if (s.row < 0 || s.row >= count) {
        s.row = -1;
        switch (m) {
            case Move::Up:
                if (s.category == 0) s.then = Then::ToBar;
                else --s.category, s.then = Then::Moved;
                return s;
            case Move::Down:
                if (s.category + 1 < cats) ++s.category, s.then = Then::Moved;
                return s;
            case Move::Right:
            case Move::Press:
                for (int r = 0; r < count; ++r)
                    if (takes(r)) {
                        s.row = r;
                        s.then = Then::Moved;
                        break;
                    }
                return s;
            case Move::Back: s.then = Then::Back; return s;
            default: return s;
        }
    }
    switch (m) {
        case Move::Up:
        case Move::Down: {
            const int d = m == Move::Down ? 1 : -1;
            int next = s.row + d;
            while (next >= 0 && next < count && !takes(next)) next += d;
            if (takes(next)) s.row = next, s.then = Then::Moved;
            return s;
        }
        case Move::Left:
        case Move::Back: s.row = -1, s.then = Then::Moved; return s;
        case Move::Press: s.then = Then::Press; return s;
        default: return s;
    }
}

BarStep bar(int at, int count, Move m, bool screenTakesFocus) {
    BarStep s;
    s.at = at;
    switch (m) {
        case Move::Left:
        case Move::Right:
        case Move::PrevScreen:
        case Move::NextScreen: {
            const int d = m == Move::Right || m == Move::NextScreen ? 1 : -1;
            const int next = at + d;
            if (next < 0 || next >= count) s.edge = true;
            else s.at = next;
            return s;
        }
        case Move::Down:
        case Move::Press:
        case Move::Back:
            // Neither leaves for a screen with nothing to focus: focus would go where nobody can see it.
            if (screenTakesFocus) s.leave = true;
            else s.edge = true;
            return s;
        case Move::Up:
            s.edge = true;
            return s;
    }
    return s;
}

int topFor(int focus, int top, int count, int rows) {
    const int most = std::max(0, count - rows);
    if (focus < top) top = focus;
    else if (focus >= top + rows) top = focus - rows + 1;
    return std::clamp(top, 0, most);
}

}  // namespace moves
