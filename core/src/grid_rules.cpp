#include "grid_rules.hpp"

namespace core::grid_rules
{
bool try_move_player(internal::Map& map, int dx, int dy)
{
    const internal::Point player = internal::find_player(map);
    const internal::Player moved_player = internal::Player::from_delta(dx, dy);

    const internal::Point next {player.x + dx, player.y + dy};
    if (!internal::in_bounds(map, next)) return false;

    const internal::Object next_object = map.grid[next.y][next.x];
    const auto next_props = next_object.properties();
    if (next_props.blocks_movement) return false;

    if (next_props.is_empty)
    {
        map.grid[player.y][player.x] = internal::Object(internal::Empty {});
        map.grid[next.y][next.x] = internal::Object(moved_player);
        return true;
    }

    if (!next_props.is_pushable) return false;

    const internal::Point pushed {next.x + dx, next.y + dy};
    if (!internal::in_bounds(map, pushed)) return false;

    const internal::Object pushed_object = map.grid[pushed.y][pushed.x];
    if (!pushed_object.properties().is_empty) return false;

    map.grid[pushed.y][pushed.x] = next_object;
    map.grid[next.y][next.x] = internal::Object(moved_player);
    map.grid[player.y][player.x] = internal::Object(internal::Empty {});
    return true;
}

bool map_commit(internal::History& history)
{
    auto& map = internal::current(history);
    if (map.commits_left <= 0) return false;

    const int commits_after = map.commits_left - 1;
    internal::commit(history);
    internal::current(history).commits_left = commits_after;
    return true;
}

bool map_undo(internal::History& history)
{
    auto& map = internal::current(history);
    if (map.undos_left <= 0) return false;
    if (history.size() <= 1) return false;

    const int undos_after = map.undos_left - 1;
    internal::undo(history);
    internal::current(history).undos_left = undos_after;
    return true;
}
}  // namespace core::grid_rules
