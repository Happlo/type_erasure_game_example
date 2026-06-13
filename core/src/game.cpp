#include "core/game.hpp"

#include "game_factory.hpp"
#include "game_model.hpp"
#include "grid_rules.hpp"
#include "map_io.hpp"
#include "solution_rules.hpp"

#include <algorithm>
#include <memory>

namespace core
{
namespace
{
struct GridText
{
    std::string text;
    Location origin;
};

GridText map_to_grid_text(const internal::Map& map)
{
    if (map.objects.empty())
        return {};

    auto first = map.objects.begin();
    int min_x = first->first.x;
    int max_x = first->first.x;
    int min_y = first->first.y;
    int max_y = first->first.y;

    for (const auto &[location, object] : map.objects)
    {
        (void)object;
        min_x = std::min(min_x, location.x);
        max_x = std::max(max_x, location.x);
        min_y = std::min(min_y, location.y);
        max_y = std::max(max_y, location.y);
    }

    const int width = max_x - min_x + 1;
    const int height = max_y - min_y + 1;
    std::string out;
    out.reserve(static_cast<size_t>(height * (width + 1)));

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const auto object = map.objects.find(Location{.x = min_x + x, .y = min_y + y});
            out.push_back(object == map.objects.end() ? ' ' : object->second.view().symbol);
        }
        if (y + 1 < height) out.push_back('\n');
    }

    return GridText{.text = out, .origin = Location{.x = min_x, .y = min_y}};
}

GameResult evaluate_map(const internal::Map& map)
{
    const GridText grid = map_to_grid_text(map);
    GameResult result = solution_rules::evaluate_equation(grid.text);
    if (grid.origin.x == 0 && grid.origin.y == 0)
        return result;

    std::map<Location, EqualityStatus> shifted;
    for (const auto &[location, status] : result.equal_sign_status)
    {
        shifted[Location{.x = location.x + grid.origin.x, .y = location.y + grid.origin.y}] =
            status;
    }
    result.equal_sign_status = std::move(shifted);
    return result;
}

class DefaultGame final : public Game
{
public:
    explicit DefaultGame(internal::Map map) : history_ {std::move(map)}
    {}

    GameResult apply_event(const Event event) override
    {
        auto& map = internal::current(history_);
        [&]()
        {
        switch (event)
        {
            case Event::MoveUp: return grid_rules::try_move_player(map, 0, -1);
            case Event::MoveLeft: return grid_rules::try_move_player(map, -1, 0);
            case Event::MoveDown: return grid_rules::try_move_player(map, 0, 1);
            case Event::MoveRight: return grid_rules::try_move_player(map, 1, 0);
            case Event::PickItem: return grid_rules::try_pick_item(map);
            case Event::DropItem: return grid_rules::try_drop_item(map);
            case Event::Commit: return grid_rules::map_commit(history_);
            case Event::Undo: return grid_rules::map_undo(history_);
        }

        return false;
        }();

        last_result_ = evaluate_map(internal::current(history_));
        return last_result_;
    }

    MapView view() const override
    {
        return build_view(internal::current(history_));
    }

private:
    internal::History history_;
    GameResult last_result_ {evaluate_map(internal::current(history_))};
};

}  // namespace

namespace internal
{
std::unique_ptr<Game> make_game(Map map)
{
    return std::make_unique<DefaultGame>(std::move(map));
}
}  // namespace internal

std::unique_ptr<Game> Game::load_from_file(const std::filesystem::path &path)
{
    return internal::make_game(map_io::map_from_file(path));
}
}  // namespace core
