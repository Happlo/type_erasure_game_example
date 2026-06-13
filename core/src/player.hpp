#pragma once

#include "core/map.hpp"
#include "game_model.hpp"

#include <optional>

namespace core::internal
{
Facing facing_from_delta(int dx, int dy);

char glyph(Facing facing);

char glyph(const PlayerState &player);

std::optional<Location> find_player(const Map &map);
std::optional<core::Player> get_public_player(const Map &map);
} // namespace core::internal
