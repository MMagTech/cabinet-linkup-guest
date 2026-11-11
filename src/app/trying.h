// `2pstart --try <file>`: steps from a file played into the app as if somebody's hands made them, with
// pictures of the window saved along the way. For trying the app with nobody at it. Each step goes
// through App::handle, as a real press does.
//
//   wait 1.5             seconds
//   key down             a key, by SDL's name for it: Down, Return, Escape, Tab, Backspace, F11
//   type alex            typed
//   pad a                a controller button pressed and let go: a, b, up, down, left, right, lb, rb
//   point 960 540        the mouse moved to there on the console's 1920 by 1080 canvas
//   click 960 540        and clicked; `rightclick` clicks the right button
//   shot first-run.png   a picture of the window as it is
//   quit
//
// Blank lines and lines starting with # are skipped.

#pragma once

#include <deque>
#include <string>

#include <SDL3/SDL.h>

class App;

class Trying {
public:
    bool load(const std::string& path, std::string& why);
    // Plays the steps that are due after `dt` more seconds. False once it says quit or runs out.
    bool play(float dt, App& app, SDL_Renderer* renderer);
    // A picture is wanted of this frame: taken after it is drawn and before it is shown.
    void takeShot(SDL_Renderer* renderer);

private:
    std::deque<std::string> steps_;
    float waiting_ = 0.0f;
    std::string shot_;
};
