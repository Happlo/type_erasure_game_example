#include "core/game.hpp"

#include "game_factory.hpp"
#include "game_model.hpp"
#include "grid_rules.hpp"
#include "map_io.hpp"
#include "solution_rules.hpp"

#include <algorithm>
#include <memory>
#include <set>

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

std::vector<Location> equal_sign_locations(
    const std::map<Location, solution_rules::EqualityStatus>& equal_sign_status)
{
    std::vector<Location> locations;
    locations.reserve(equal_sign_status.size());
    for (const auto& [location, status] : equal_sign_status)
    {
        (void)status;
        locations.push_back(location);
    }
    return locations;
}

solution_rules::EquationResult evaluate_map(const internal::Map& map)
{
    const GridText grid = map_to_grid_text(map);
    solution_rules::EquationResult result = solution_rules::evaluate_equation(grid.text);
    if (grid.origin.x == 0 && grid.origin.y == 0)
        return result;

    std::map<Location, solution_rules::EqualityStatus> shifted;
    for (const auto& [location, status] : result.equal_sign_status)
        shifted[{location.x + grid.origin.x, location.y + grid.origin.y}] = status;
    result.equal_sign_status = std::move(shifted);
    return result;
}

GameResult current_result(const internal::Map& map)
{
    const solution_rules::EquationResult evaluation = evaluate_map(map);
    GameResult result;
    result.resolved_variables = evaluation.resolved_variables;
    result.remaining_equal_signs = equal_sign_locations(evaluation.equal_sign_status);
    return result;
}

GameResult previous_state(internal::History& history)
{
    grid_rules::map_undo(history);
    return current_result(internal::current(history));
}

bool try_apply_action(internal::Map& map, const Event event)
{
    switch (event)
    {
        case Event::MoveUp: return grid_rules::try_move_player(map, 0, -1);
        case Event::MoveLeft: return grid_rules::try_move_player(map, -1, 0);
        case Event::MoveDown: return grid_rules::try_move_player(map, 0, 1);
        case Event::MoveRight: return grid_rules::try_move_player(map, 1, 0);
        case Event::PickItem: return grid_rules::try_pick_item(map);
        case Event::DropItem: return grid_rules::try_drop_item(map);
        case Event::Undo: return false;
    }

    return false;
}

std::map<Location, Object> remove_solved_equations(internal::Map& map,
                                                   const solution_rules::EquationResult& result)
{
    std::set<Location> objects_to_remove;

    for (const auto& [equal_sign, status] : result.equal_sign_status)
    {
        if (status != solution_rules::EqualityStatus::Equal)
            continue;

        int first_x = equal_sign.x;
        while (true)
        {
            const auto object = map.objects.find(Location{.x = first_x - 1, .y = equal_sign.y});
            if (object == map.objects.end() || object->second.view().symbol == '#')
                break;
            --first_x;
        }

        int last_x = equal_sign.x;
        while (true)
        {
            const auto object = map.objects.find(Location{.x = last_x + 1, .y = equal_sign.y});
            if (object == map.objects.end() || object->second.view().symbol == '#')
                break;
            ++last_x;
        }

        for (int x = first_x; x <= last_x; ++x)
            objects_to_remove.insert(Location{.x = x, .y = equal_sign.y});
    }

    std::map<Location, Object> removed_objects;
    for (const Location& location : objects_to_remove)
    {
        const auto object = map.objects.find(location);
        if (object == map.objects.end())
            continue;
        removed_objects.emplace(location, object->second.view());
        map.objects.erase(object);
    }
    return removed_objects;
}

class DefaultGame final : public Game
{
public:
    explicit DefaultGame(internal::Map map) : history_ {std::move(map)}
    {}

    GameResult apply_event(const Event event) override
    {
        if (event == Event::Undo)
            return last_result_ = previous_state(history_);

        internal::Map next = internal::current(history_);
        if (!try_apply_action(next, event))
            return last_result_ = current_result(internal::current(history_));

        const solution_rules::EquationResult evaluation = evaluate_map(next);
        last_result_.resolved_variables = evaluation.resolved_variables;
        last_result_.removed_objects = remove_solved_equations(next, evaluation);
        last_result_.remaining_equal_signs =
            equal_sign_locations(evaluate_map(next).equal_sign_status);
        internal::commit(history_, std::move(next));
        return last_result_;
    }

    MapView view() const override
    {
        return build_view(internal::current(history_));
    }

private:
    internal::History history_;
    GameResult last_result_ {current_result(internal::current(history_))};
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
