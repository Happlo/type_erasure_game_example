#include "player.hpp"

#include <stdexcept>

namespace core::internal
{
PlayerState PlayerState::from_delta(const int dx, const int dy, core::Player player)
{
    auto result = PlayerState{.player = std::move(player), .facing = facing_from_delta(dx, dy)};
    result.player.symbol = glyph(result);
    return result;
}

Facing facing_from_delta(const int dx, const int dy)
{
    if (dx == 0 && dy < 0)
        return Facing::North;
    if (dx == 0 && dy > 0)
        return Facing::South;
    if (dx < 0 && dy == 0)
        return Facing::West;
    if (dx > 0 && dy == 0)
        return Facing::East;
    return Facing::South;
}

char glyph(const Facing facing)
{
    switch (facing)
    {
    case Facing::North:
        return '^';
    case Facing::South:
        return 'v';
    case Facing::West:
        return '<';
    case Facing::East:
        return '>';
    }
    return '@';
}

char glyph(const PlayerState &player) { return glyph(player.facing); }

core::CellView view(const PlayerState &value) { return value.player; }

std::optional<Point> find_player(const Map &map)
{
    for (int y = 0; y < grid_height(map); ++y)
    {
        for (int x = 0; x < grid_width(map); ++x)
        {
            if (map.grid[y][x].is<PlayerState>())
                return Point{x, y};
        }
    }
    return std::nullopt;
}

std::optional<core::Player> get_public_player(const Map &map)
{
    const std::optional<Point> player_position = find_player(map);
    if (!player_position.has_value())
        return std::nullopt;

    const PlayerState player_state =
        map.grid[static_cast<size_t>(player_position->y)][static_cast<size_t>(player_position->x)]
            .get<PlayerState>();
    core::Player player = player_state.player;
    player.symbol = glyph(player_state.facing);
    return player;
}
} // namespace core::internal
