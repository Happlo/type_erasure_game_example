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

std::optional<Location> find_player(const Map &map)
{
    if (!map.player.has_value())
        return std::nullopt;
    return Location{.x = map.player->player.location.x, .y = map.player->player.location.y};
}

std::optional<core::Player> get_public_player(const Map &map)
{
    if (!map.player.has_value())
        return std::nullopt;

    core::Player player = map.player->player;
    player.symbol = glyph(map.player->facing);
    return player;
}
} // namespace core::internal
