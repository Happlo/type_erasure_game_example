#include "grid_rules.hpp"
#include "core/location.hpp"

#include "player.hpp"

namespace core::grid_rules
{
namespace
{
Location Location_in_front(const Location &Location, internal::Facing facing)
{
    switch (facing)
    {
    case internal::Facing::North:
        return {Location.x, Location.y - 1};
    case internal::Facing::South:
        return {Location.x, Location.y + 1};
    case internal::Facing::West:
        return {Location.x - 1, Location.y};
    case internal::Facing::East:
        return {Location.x + 1, Location.y};
    }

    return Location;
}
} // namespace

bool try_move_player(internal::Map &map, int dx, int dy)
{
    const std::optional<Location> player = internal::find_player(map);
    if (!player.has_value() || !map.player.has_value())
        return false;
    const internal::PlayerState moved_player =
        internal::PlayerState::from_delta(dx, dy, map.player->player);

    const Location next{player->x + dx, player->y + dy};
    if (!internal::in_bounds(map, next))
        return false;

    const internal::TypeErasedObject next_object = map.grid[next.y][next.x];
    const auto next_view = next_object.view();
    const auto *next_object_props = std::get_if<core::Object>(&next_view);

    if (std::holds_alternative<core::Empty>(next_view))
    {
        map.player = moved_player;
        map.player->player.location = core::Location{.x = next.x, .y = next.y};
        return true;
    }

    if (next_object_props == nullptr ||
        next_object_props->manipulation_level == core::Object::ManipulationLevel::None)
        return false;

    const Location pushed{next.x + dx, next.y + dy};
    if (!internal::in_bounds(map, pushed))
        return false;

    const internal::TypeErasedObject pushed_object = map.grid[pushed.y][pushed.x];
    if (!pushed_object.is<core::Empty>())
        return false;

    map.grid[pushed.y][pushed.x] = next_object;
    map.grid[next.y][next.x] = internal::TypeErasedObject(Empty{});
    map.player = moved_player;
    map.player->player.location = core::Location{.x = next.x, .y = next.y};
    return true;
}

bool try_pick_item(internal::Map &map)
{
    const std::optional<Location> player = internal::find_player(map);
    if (!player.has_value() || !map.player.has_value())
        return false;
    internal::PlayerState player_state = *map.player;
    const Location front = Location_in_front(*player, player_state.facing);
    if (!internal::in_bounds(map, front))
        return false;

    const core::CellView front_view = map.grid[front.y][front.x].view();
    const auto *item = std::get_if<core::Object>(&front_view);
    if (item == nullptr || item->manipulation_level != core::Object::ManipulationLevel::Pick)
        return false;

    player_state.player.inventory.push_back(*item);
    map.player = player_state;
    map.grid[front.y][front.x] = internal::TypeErasedObject(Empty{});
    return true;
}

bool try_drop_item(internal::Map &map)
{
    const std::optional<Location> player = internal::find_player(map);
    if (!player.has_value() || !map.player.has_value())
        return false;
    internal::PlayerState player_state = *map.player;
    if (player_state.player.inventory.empty())
        return false;

    const Location front = Location_in_front(*player, player_state.facing);
    if (!internal::in_bounds(map, front))
        return false;
    if (!map.grid[front.y][front.x].is<core::Empty>())
        return false;

    const core::Object item = player_state.player.inventory.back();
    player_state.player.inventory.pop_back();
    map.player = player_state;
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
