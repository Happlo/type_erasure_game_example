#include "game_model.hpp"

namespace core::internal
{
PlayerState PlayerState::from_delta(const int dx, const int dy, core::Player player)
{
    return {.player = std::move(player), .facing = facing_from_delta(dx, dy)};
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

char glyph(const core::Empty &) { return ' '; }

char glyph(const int &value)
{
    if (value < 0 || 9 < value)
    {
        throw std::runtime_error("int must be between 0 and 9 to generate a glyph");
    }
    return static_cast<char>('0' + value);
}

char glyph(const char &value) { return value; }

char glyph(const core::Object &value) { return value.symbol; }

core::CellView TypeErasedObject::view() const { return self_->view_(); }

int grid_height(const Map &map) { return static_cast<int>(map.grid.size()); }

int grid_width(const Map &map)
{
    if (map.grid.empty())
        return 0;
    return static_cast<int>(map.grid[0].size());
}

bool in_bounds(const Map &map, const Point &point)
{
    return point.x >= 0 && point.y >= 0 && point.x < grid_width(map) && point.y < grid_height(map);
}

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

void commit(History &history)
{
    assert(!history.empty());
    history.push_back(history.back());
}

void undo(History &history)
{
    assert(!history.empty());
    history.pop_back();
}

Map &current(History &history)
{
    assert(!history.empty());
    return history.back();
}

const Map &current(const History &history)
{
    assert(!history.empty());
    return history.back();
}

core::MapView build_view(const Map &map)
{
    core::MapView view;
    view.width = grid_width(map);
    view.height = grid_height(map);
    view.commits_left = map.commits_left;
    view.undos_left = map.undos_left;
    view.player = get_public_player(map);
    view.cells.reserve(static_cast<size_t>(view.width * view.height));

    for (int y = 0; y < view.height; ++y)
    {
        for (int x = 0; x < view.width; ++x)
        {
            view.cells.push_back(map.grid[static_cast<size_t>(y)][static_cast<size_t>(x)].view());
        }
    }

    return view;
}
} // namespace core::internal
