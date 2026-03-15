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

char glyph(const PlayerState &player)
{
    return glyph(player.facing);
}

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

core::CellView Object::view() const { return self_->view_(); }

bool Object::is_empty() const { return self_->is_empty_(); }

bool Object::is_player() const { return self_->is_player_(); }

PlayerState Object::player_state() const { return self_->player_state_(); }

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

Point find_player(const Map &map)
{
    for (int y = 0; y < grid_height(map); ++y)
    {
        for (int x = 0; x < grid_width(map); ++x)
        {
            if (map.grid[y][x].is_player())
                return {x, y};
        }
    }
    throw std::runtime_error("Player not found");
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

Map make_map()
{
    constexpr size_t width = 9;
    constexpr size_t height = 7;

    Map map;
    map.grid.assign(height, std::vector<Object>(width, Empty{}));

    map.grid[6][0] = MakeObject(PlayerState{});
    map.grid[1][2] = MakeObject('+').pushable();
    map.grid[1][4] = MakeObject('=').pushable();

    map.grid[4][4] = MakeObject(1).pushable();
    map.grid[5][7] = MakeObject(2).pushable();
    map.grid[3][6] = MakeObject(3).pushable();
    map.grid[5][2] = MakeObject(4).pushable();

    return map;
}

core::MapView build_view(const Map &map)
{
    core::MapView view;
    view.width = grid_width(map);
    view.height = grid_height(map);
    view.commits_left = map.commits_left;
    view.undos_left = map.undos_left;
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
