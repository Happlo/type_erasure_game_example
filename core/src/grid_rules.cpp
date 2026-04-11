#include "grid_rules.hpp"

namespace core::grid_rules
{
namespace
{
internal::Point point_in_front(const internal::Point &point, internal::Facing facing)
{
    switch (facing)
    {
    case internal::Facing::North:
        return {point.x, point.y - 1};
    case internal::Facing::South:
        return {point.x, point.y + 1};
    case internal::Facing::West:
        return {point.x - 1, point.y};
    case internal::Facing::East:
        return {point.x + 1, point.y};
    }

    return point;
}
} // namespace

bool try_move_player(internal::Map &map, int dx, int dy)
{
    const std::optional<internal::Point> player = internal::find_player(map);
    if (!player.has_value())
        return false;
    const internal::PlayerState current_player =
        map.grid[player->y][player->x].get<internal::PlayerState>();
    const internal::PlayerState moved_player =
        internal::PlayerState::from_delta(dx, dy, current_player.player);

    const internal::Point next{player->x + dx, player->y + dy};
    if (!internal::in_bounds(map, next))
        return false;

    const internal::TypeErasedObject next_object = map.grid[next.y][next.x];
    const auto next_view = next_object.view();
    const auto *next_object_props = std::get_if<core::Object>(&next_view);

    if (std::holds_alternative<core::Empty>(next_view))
    {
        map.grid[player->y][player->x] = internal::TypeErasedObject(Empty{});
        map.grid[next.y][next.x] = internal::TypeErasedObject(moved_player);
        return true;
    }

    if (next_object_props == nullptr ||
        next_object_props->manipulation_level == core::Object::ManipulationLevel::None)
        return false;

    const internal::Point pushed{next.x + dx, next.y + dy};
    if (!internal::in_bounds(map, pushed))
        return false;

    const internal::TypeErasedObject pushed_object = map.grid[pushed.y][pushed.x];
    if (!pushed_object.is<core::Empty>())
        return false;

    map.grid[pushed.y][pushed.x] = next_object;
    map.grid[next.y][next.x] = internal::TypeErasedObject(moved_player);
    map.grid[player->y][player->x] = internal::TypeErasedObject(Empty{});
    return true;
}

bool try_pick_item(internal::Map &map)
{
    const std::optional<internal::Point> player = internal::find_player(map);
    if (!player.has_value())
        return false;
    internal::PlayerState player_state =
        map.grid[player->y][player->x].get<internal::PlayerState>();
    const internal::Point front = point_in_front(*player, player_state.facing);
    if (!internal::in_bounds(map, front))
        return false;

    const core::CellView front_view = map.grid[front.y][front.x].view();
    const auto *item = std::get_if<core::Object>(&front_view);
    if (item == nullptr || item->manipulation_level != core::Object::ManipulationLevel::Pick)
        return false;

    player_state.player.inventory.push_back(*item);
    map.grid[player->y][player->x] = internal::TypeErasedObject(player_state);
    map.grid[front.y][front.x] = internal::TypeErasedObject(Empty{});
    return true;
}

bool try_drop_item(internal::Map &map)
{
    const std::optional<internal::Point> player = internal::find_player(map);
    if (!player.has_value())
        return false;
    internal::PlayerState player_state =
        map.grid[player->y][player->x].get<internal::PlayerState>();
    if (player_state.player.inventory.empty())
        return false;

    const internal::Point front = point_in_front(*player, player_state.facing);
    if (!internal::in_bounds(map, front))
        return false;
    if (!map.grid[front.y][front.x].is<core::Empty>())
        return false;

    const core::Object item = player_state.player.inventory.back();
    player_state.player.inventory.pop_back();
    map.grid[player->y][player->x] = internal::TypeErasedObject(player_state);
    map.grid[front.y][front.x] = internal::TypeErasedObject(item);
    return true;
}

bool map_commit(internal::History &history)
{
    auto &map = internal::current(history);
    if (map.commits_left <= 0)
        return false;

    const int commits_after = map.commits_left - 1;
    internal::commit(history);
    internal::current(history).commits_left = commits_after;
    return true;
}

bool map_undo(internal::History &history)
{
    auto &map = internal::current(history);
    if (map.undos_left <= 0)
        return false;
    if (history.size() <= 1)
        return false;

    const int undos_after = map.undos_left - 1;
    internal::undo(history);
    internal::current(history).undos_left = undos_after;
    return true;
}
} // namespace core::grid_rules
