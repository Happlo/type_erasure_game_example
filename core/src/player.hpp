#pragma once

#include "core/map.hpp"
#include "game_model.hpp"

#include <optional>

namespace core::internal
{
enum class Facing
{
    North,
    South,
    West,
    East
};

struct PlayerState
{
    core::Player player{};
    Facing facing{Facing::South};

    static PlayerState from_delta(int dx, int dy, core::Player player = {});
};

Facing facing_from_delta(int dx, int dy);

char glyph(Facing facing);

char glyph(const PlayerState &player);

core::CellView view(const PlayerState &value);

std::optional<Point> find_player(const Map &map);
std::optional<core::Player> get_public_player(const Map &map);
} // namespace core::internal
