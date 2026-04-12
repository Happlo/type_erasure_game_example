#include "game_model.hpp"

#include "player.hpp"

namespace core::internal
{
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
