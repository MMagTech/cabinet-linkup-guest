// 2P Start's window: what is on screen, and what each press does. It is the friend's side of the
// console's LinkUp screens (linkup.cpp, linkupflow.cpp and linkupsession.cpp in CabinetOS): first
// run in place of the console's Add a server, the list of friends, Settings, the panels over them,
// and a game. The server is guest::Link's, the game's connection is game::Game's, and the drawing is
// the screens'; this joins them. The game's part is session.cpp.
//
// Everything here runs on the window's thread. Nothing in it waits on the network.

#pragma once

#include <functional>
#include <map>
#include <optional>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "app/idle.h"
#include "app/input.h"
#include "app/notice.h"
#include "app/textline.h"
#include "game/game.h"
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
    // A controller pressed with another program in front: true when `e` is one, which the menus do not
    // take (idle.h). Before handle.
    bool elsewhere(const SDL_Event& e);
    // The window's close button in a game: true when `e` is that, and the game is paused or the
    // question to leave is asked, as Start asks, rather than the game going on hidden. Before the tray.
    bool closeInGame(const SDL_Event& e);
    void tick(float dt);
    // The frame, not yet shown: the caller presents it.
    void draw();
    // Something moves or counts down, so frames are wanted. Otherwise the window can sleep.
    bool busy() const;
    // The game's picture is all there is: a frame is drawn only when a new picture has come, as soon
    // as it has, with no wait for the screen (linkupPictureOnly).
    bool live() const;
    // A new picture came this tick.
    bool fresh() const { return fresh_; }
    // Signs out, a few seconds at the most.
    void quit();

    // For the tray (tray.h). An invite heard while nobody may be looking goes to `onInvite` too: who,
    // the console's line for it, and the game.
    void tellInvites(std::function<void(const std::string&, const std::string&, const std::string&)> onInvite);
    // Join or Decline for this friend's invite, as pressing their tile opens it. Nothing when it ended.
    void answerFrom(const std::string& who);
    std::string statusLine() const;
    void flipStatus();
    // True once ever, and kept: the first time the window hides to the tray, it is said.
    bool sayStillRunning();

private:
    enum class Place { FirstRun, Friends, Settings, Game };

    // What is over the screen: a question or a line (Panel), a list to read (the credits, How to),
    // or a line to type.
    struct Overlay {
        enum class Kind { None, Panel, Credits, HowTo, Field };
        bool reading() const { return kind == Kind::Credits || kind == Kind::HowTo; }
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

    // A game (session.cpp), the friend's side of linkupsession.cpp, each named for the function there.
    void fromGame();
    void tickSession(float dt);
    void sendPad(float dt);
    bool takeGameEvent(const SDL_Event& e);
    void moveInGame(moves::Move m);
    void fromPeer(const std::string& word, const std::string& game);
    void connected();
    void connectFailed(const std::string& reason);
    void friendPicture(bool on);
    void askLeave();
    void friendAsksPause();
    void friendLeaves();
    void idlePause();
    void peerLost();
    void peerBack();
    void peerEndedSession();
    void sessionClosed();
    void showWaiting(const std::string& title, const std::string& under);
    bool pictureBehind() const;
    void takePicture();
    void drawPicture();
    SDL_Gamepad* playingPad() const;

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
    void aboutPanel(int focus);
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
    void setStartup(bool on);
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
    float pointerStill_ = 0.0f;   // since the mouse last moved
    bool pointerOff_ = false;     // hidden over the game's picture
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

    // A game, as the friend's console holds one (linkupSide, linkupPlay and the rest in
    // linkupsession.cpp): from Join until it ends, and past that only while a join left before its
    // answer waits to be ended.
    enum class Shows { Waiting, Picture, Reconnecting };
    struct Session {
        std::string host, game;       // who hosts, and the game, as the host last named it
        std::string invite, id;       // the invite answered, and the session the server opened
        bool left = false;            // left before the join was answered: ended once it is
        bool plays = false;           // this computer can play it (game::Game::canPlay)
        bool streamUp = false;
        bool showsPicture = false;    // the host's picture is on
        bool pausedHere = false;      // a panel here has the game stopped for both
        bool pausedThere = false;
        bool lost = false;
        float lostFor = 0.0f;
        Shows shows = Shows::Waiting;
        std::string title;            // what Waiting and Reconnecting say
        std::string under;            // the game, under it
        bool resumes = false;         // the host paused, and either can resume
        linkupwire::Pad sent;
        uint16_t seq = 0;
        float padAge = 0.0f;
        bool pressHere = false;
        float pressAge = 0.0f;
        bool l3 = false, r3 = false;
    };
    std::optional<Session> session_;
    std::unique_ptr<game::Game> game_;
    SDL_Texture* picture_ = nullptr;
    bool fresh_ = false;
    bool liveDrawn_ = false;     // the swap wait is off for the picture

    std::vector<SDL_JoystickID> padIds_;
    std::map<SDL_JoystickID, SDL_Gamepad*> pads_;

    // Nobody at the computer: Offline to friends, as a console left alone (idle.h).
    idle::Watch idle_;
    bool away_ = false;
    std::function<void(const std::string&, const std::string&, const std::string&)> onInvite_;
};
