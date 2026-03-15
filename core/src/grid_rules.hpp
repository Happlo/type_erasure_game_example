#pragma once

#include "game_model.hpp"

namespace core::grid_rules
{
bool try_move_player(internal::Map& map, int dx, int dy);
bool try_pick_item(internal::Map& map);
bool try_drop_item(internal::Map& map);
bool map_commit(internal::History& history);
bool map_undo(internal::History& history);
}  // namespace core::grid_rules
