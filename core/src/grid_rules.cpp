#include "grid_rules.hpp"

namespace core::grid_rules
{
bool try_move_player(internal::World& world, int dx, int dy)
{
    const internal::Point player = internal::find_player(world);
    const internal::Player moved_player = internal::Player::from_delta(dx, dy);

    const internal::Point next {player.x + dx, player.y + dy};
    if (!internal::in_bounds(world, next)) return false;

    const internal::Object next_object = world.grid[next.y][next.x];
    const auto next_props = next_object.properties();
    if (next_props.blocks_movement) return false;

    if (next_props.is_empty)
    {
        world.grid[player.y][player.x] = internal::Object(internal::Empty {});
        world.grid[next.y][next.x] = internal::Object(moved_player);
        return true;
    }

    if (!next_props.is_pushable) return false;

    const internal::Point pushed {next.x + dx, next.y + dy};
    if (!internal::in_bounds(world, pushed)) return false;

    const internal::Object pushed_object = world.grid[pushed.y][pushed.x];
    if (!pushed_object.properties().is_empty) return false;

    world.grid[pushed.y][pushed.x] = next_object;
    world.grid[next.y][next.x] = internal::Object(moved_player);
    world.grid[player.y][player.x] = internal::Object(internal::Empty {});
    return true;
}

bool world_commit(internal::History& history)
{
    auto& world = internal::current(history);
    if (world.commits_left <= 0) return false;

    const int commits_after = world.commits_left - 1;
    internal::commit(history);
    internal::current(history).commits_left = commits_after;
    return true;
}

bool world_undo(internal::History& history)
{
    auto& world = internal::current(history);
    if (world.undos_left <= 0) return false;
    if (history.size() <= 1) return false;

    const int undos_after = world.undos_left - 1;
    internal::undo(history);
    internal::current(history).undos_left = undos_after;
    return true;
}
}  // namespace core::grid_rules
