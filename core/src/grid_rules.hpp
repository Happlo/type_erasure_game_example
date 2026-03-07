#pragma once

#include "game_model.hpp"

namespace core::grid_rules
{
bool try_move_player(internal::World& world, int dx, int dy);
bool world_commit(internal::History& history);
bool world_undo(internal::History& history);
}  // namespace core::grid_rules
