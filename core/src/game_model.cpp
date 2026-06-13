#include "game_model.hpp"

#include "player.hpp"

namespace core::internal
{
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
    view.commits_left = map.commits_left;
    view.undos_left = map.undos_left;
    view.player = get_public_player(map);
    view.objects.clear();
    for (const auto &[location, object] : map.objects)
        view.objects.insert_or_assign(location, object.view());
    return view;
}
} // namespace core::internal
