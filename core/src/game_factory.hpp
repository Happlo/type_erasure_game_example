#pragma once

#include "core/game.hpp"
#include "game_model.hpp"

#include <memory>

namespace core::internal
{
std::unique_ptr<Game> make_game(Map map);
}
