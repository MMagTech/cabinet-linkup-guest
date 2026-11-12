// Check for updates, the row in Settings: whether the project's Releases have a newer 2P Start than
// this one. Asked a minute after the app starts, once a day after that, and when the row is pressed;
// nothing is downloaded or changed. When there is one, Download opens its page in the browser, where
// the friend takes the Setup or the DMG as they did the first time. The words are the console's
// (updateflow.cpp): "Up to date", "Update available".
//
// The one place it asks is GitHub's API, for the project's latest release: its tag ("v0.2.0") is
// the version, and its page is where it is downloaded.

#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace updates {

constexpr const char* kLatest = "https://api.github.com/repos/MMagTech/cabinet-linkup-guest/releases/latest";

struct Release {
    std::string version;   // "0.2.0"
    std::string page;      // where it is downloaded
};

// GitHub's answer, read: nothing when it is not a release.
std::optional<Release> read(const std::string& answer);
// Whether `theirs` comes after `ours`: the numbers between the dots, compared in turn.
bool newer(const std::string& theirs, const std::string& ours);

enum class State { None, Checking, UpToDate, Available, Failed };

// One check at a time, on a thread of its own, so the window never waits on it. `wake` is called from
// that thread when it ends, to stop the window sleeping.
class Check {
public:
    Check(std::string ours, std::function<void()> wake);
    ~Check();
    Check(const Check&) = delete;
    Check& operator=(const Check&) = delete;

    // Nothing while one runs.
    void start();
    // True once for each check that ended since the last call, on the window's thread.
    bool ended();
    State state() const;
    // The newer release, while state() is Available.
    Release found() const;

private:
    void run();

    std::string ours_;
    std::function<void()> wake_;
    std::thread thread_;
    mutable std::mutex mu_;
    State state_ = State::None;
    Release found_;
    bool ended_ = false;
};

}  // namespace updates
