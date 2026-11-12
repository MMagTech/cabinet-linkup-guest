// What Undo goes back through in a line being typed (textline.h): the line as it was before each
// change, its caret and chosen letters with it. Letters typed one after another are one change, as
// any app's text box has it, and so are Backspaces or Deletes one after another; a paste, a cut, or
// the caret moved between starts another. A line new to typing has nothing to undo.

#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "app/textline.h"

class Undo {
public:
    enum class Change { Typed, Erased, Other };

    // `was` is about to be changed by `how`.
    void before(const textline::Line& was, Change how) {
        if (how != Change::Other && how == last_ && !was_.empty()) return;
        was_.push_back(was);
        if (was_.size() > kMost) was_.erase(was_.begin());
        last_ = how;
    }
    // The caret moved, or letters were chosen, with nothing changed: what comes next is a change of
    // its own.
    void moved() { last_ = Change::Other; }
    // The line before the last change, which is then forgotten: nothing when there is none.
    std::optional<textline::Line> back() {
        last_ = Change::Other;
        if (was_.empty()) return std::nullopt;
        textline::Line l = std::move(was_.back());
        was_.pop_back();
        return l;
    }
    void clear() {
        was_.clear();
        last_ = Change::Other;
    }

private:
    static constexpr size_t kMost = 100;
    std::vector<textline::Line> was_;
    Change last_ = Change::Other;
};
