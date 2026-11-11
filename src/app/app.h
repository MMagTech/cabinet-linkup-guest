// 2P Start's window: what is on screen, and what each press does. It is the friend's side of the
// console's LinkUp screens (linkup.cpp, linkupflow.cpp and linkupsession.cpp in CabinetOS): first
// run in place of the console's Add a server, the list of friends, Settings, and the panels over
// them. The server is guest::Link's; the drawing is the screens'; this joins the two.
//
// Everything here runs on the window's thread. Nothing in it waits on the network.

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "app/input.h"
#include "app/notice.h"
#include "app/textline.h"
#include "guest/link.h"
#include "guest/settings.h"
#include "linkuprules.h"
#include "screens/screens.h"
#include "ui/draw.h"
#include "ui/parts.h"

class App {
public:
    // `wake` is an SDL event type the server's threads push to stop the window sleeping.
    App(SDL_Window* window, SDL_Renderer* renderer, const std::string& fonts, std::string folder, Uint32 wake);
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // False, with why, when the fonts or the folder cannot be had.
    bool start(std::string& why);
    void handle(const SDL_Event& e);
    void tick(float dt);
    // The frame, not yet shown: the caller presents it.
    void draw();
    // Something moves or counts down, so frames are wanted. Otherwise the window can sleep.
    bool busy() const;
    // Signs out, a few seconds at the most.
    void quit();

private:
    enum class Place { FirstRun, Friends, Settings, Waiting };

    // What is over the screen: a question or a line (Panel), the credits, or a line to type.
    struct Overlay {
        enum class Kind { None, Panel, Credits, Field };
        Kind kind = Kind::None;
        parts::Panel panel;
        // A line and a prompt, with no answers: A, Enter or a click goes on. `after` is the prompt's
        // end ("to continue"), put in the words of whatever was touched last when it is drawn.
        bool prompt = false;
        std::string after;
        // An answer pressed, or -1 for Back.
        std::function<void(int)> then;
        // Whose invite the Join or Decline panel answers.
        std::string answering;
        // A line to type.
        parts::FieldPanel field;
        textline::Kind fieldKind = textline::Kind::Address;
        std::function<void(const std::string&)> typed;
    };

    // The server.
    void fromServer();
    void tookState(const guest::Link::Event& e);
    void tookFriends(std::vector<linkup::Friend> list);
    void tookPaired(const guest::Link::Event& e);
    void tookInviteEnded(const std::string& invite);
    void tookJoined(const guest::Link::Event& e);
    void tookRetyped(const guest::Link::Event& e);
    std::string statusWord() const;

    // Moving and pressing.
    void move(moves::Move m);
    void moveOnBar(moves::Move m);
    void moveOnFriends(moves::Move m);
    void moveOnSettings(moves::Move m);
    void moveOnFirstRun(moves::Move m);
    void moveOnOverlay(moves::Move m);
    void pressFriend();
    void pressSetting(int row);
    void stepSetting(int row, int by);
    void switchTo(Place place);
    bool screenTakesFocus() const;
    int tileCount() const;

    // The mouse.
    void pointAt(float x, float y, bool click);
    bool spotAt(float x, float y, ui::Spot& out, bool overlayOnly) const;

    // Typing.
    bool typing() const;
    std::string* typedLine(textline::Kind& kind);
    // The line being typed, changed by `how`, its caret with it.
    void edit(const std::function<textline::Line(textline::Line, textline::Kind)>& how);
    void typeText(const std::string& text);
    // The keys that edit a line, other than the arrows: false for any other key.
    bool typeKey(const SDL_KeyboardEvent& key);
    // A line new to typing: the caret at its end, where typing goes on.
    void caretToEnd();
    // A click on a line: the caret where it landed, or, off its letters, at the end of a line new to
    // typing (`fresh`).
    void clickCaret(int spot, float x, bool fresh);

    // What happens.
    void connect();
    void answerInvite(const linkup::Friend& f);
    void join(const linkup::Friend& f);
    void decline(const linkup::Friend& f);
    void serverPanel();
    void askAddress();
    void askCode(const std::string& address);
    void leave();
    void toFirstRun();
    void openPanel(parts::Panel p, std::function<void(int)> then);
    void openPrompt(const std::string& title, const std::string& detail, const std::string& after);
    void closeOverlay();
    void sayPairing(pairing::Result r);
    void setHands(parts::Hands hands);
    void setFullScreen(bool on);
    void keepSettings();

    // The controllers.
    void padsChanged();
    std::vector<std::string> padNames() const;
    int chosenPad() const;

    SDL_Window* window_;
    SDL_Renderer* renderer_;
    ui::Canvas canvas_;
    std::string folder_;
    Uint32 wake_;
    std::unique_ptr<guest::Link> link_;
    guest::Settings settings_;
    input::Translator input_;
    parts::Hands hands_ = parts::Hands::Mouse;
    bool textOn_ = false;
    float clock_ = 0.0f;
    float still_ = 0.0f;      // since the last key, for the caret
    size_t caret_ = 0;        // in the line being typed, in bytes

    Place place_ = Place::FirstRun;
    bool barFocused_ = false;
    Overlay overlay_;
    Notice notice_;

    // The server, as last heard.
    bool paired_ = false;
    std::string address_;
    linkup::State state_;
    std::vector<int> order_;   // which friend each tile is
    int friendFocus_ = -1;     // a tile, or -1 for the Status line

    screens::FirstRunView first_;
    float pairingLeft_ = 0.0f;   // first run's pairing, given up on at 0
    bool changing_ = false;      // Change server's pairing, from Settings

    std::string name_;           // as friends see this one, kept beside the key
    int settingsFocus_ = 0;
    bool typingName_ = false;
    std::string typedName_;

    // A join, until the game can be shown: asked, then ended as soon as it is answered.
    std::string joinHost_, joinGame_;
    bool joinLeft_ = false;      // Back was pressed while it was asked

    std::vector<SDL_JoystickID> padIds_;
    std::map<SDL_JoystickID, SDL_Gamepad*> pads_;
};
