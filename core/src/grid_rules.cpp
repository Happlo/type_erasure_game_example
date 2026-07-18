#include "grid_rules.hpp"
#include "core/location.hpp"

#include "player.hpp"

#include <vector>

namespace core::grid_rules
{
namespace
{
Location location_in_front(const Location &location, internal::Facing facing)
{
    switch (facing)
    {
    case internal::Facing::North:
        return {location.x, location.y - 1};
    case internal::Facing::South:
        return {location.x, location.y + 1};
    case internal::Facing::West:
        return {location.x - 1, location.y};
    case internal::Facing::East:
        return {location.x + 1, location.y};
    }

    return location;
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
    auto next_object = map.objects.find(next);
    if (next_object == map.objects.end())
    {
        map.player = moved_player;
        map.player->player.location = core::Location{.x = next.x, .y = next.y};
        return true;
    }

    std::vector<Location> objects_to_push;
    Location pushed = next;
    auto object = map.objects.find(pushed);
    while (object != map.objects.end())
    {
        if (object->second.view().manipulation_level ==
            core::Object::ManipulationLevel::None)
            return false;

        objects_to_push.push_back(pushed);
        pushed = {pushed.x + dx, pushed.y + dy};
        object = map.objects.find(pushed);
    }

    for (auto location = objects_to_push.rbegin(); location != objects_to_push.rend(); ++location)
    {
        const Location destination{location->x + dx, location->y + dy};
        auto object = map.objects.find(*location);
        map.objects.insert_or_assign(destination, object->second);
        map.objects.erase(object);
    }

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
    const Location front = location_in_front(*player, player_state.facing);
    auto item = map.objects.find(front);
    if (item == map.objects.end() ||
        item->second.view().manipulation_level != core::Object::ManipulationLevel::Pick)
        return false;

    player_state.player.inventory.push_back(item->second.view());
    map.player = player_state;
    map.objects.erase(item);
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

    const Location front = location_in_front(*player, player_state.facing);
    if (map.objects.contains(front))
        return false;

    const core::Object item = player_state.player.inventory.back();
    player_state.player.inventory.pop_back();
    map.player = player_state;
    map.objects.insert_or_assign(front, item);
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
