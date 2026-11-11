// game.h where the game is not built yet: Windows, until its picture and sound are. It can play
// nothing, so Join opens the session and ends it at once (app.cpp).

#include "game/game.h"

namespace game {

class Game::Loop {};

Game::Game(std::function<void()>) {}
Game::~Game() = default;
bool Game::canPlay() { return false; }
void Game::begin(Server) {}
void Game::ready(std::vector<std::string>, std::string, std::string) {}
void Game::signal(int, const std::string&) {}
void Game::pad(const linkupwire::Pad&) {}
void Game::tell(const std::string&) {}
void Game::end(bool) {}
std::vector<Game::Event> Game::take() { return {}; }
bool Game::takePicture(const std::function<void(const Planes&)>&) { return false; }

}  // namespace game
