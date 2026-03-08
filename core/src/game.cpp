#include "core/game.hpp"

#include "game_model.hpp"
#include "grid_rules.hpp"
#include "map_io.hpp"
#include "solution_rules.hpp"

#include <memory>

namespace core
{
namespace
{
class DefaultGame final : public Game
{
public:
    explicit DefaultGame(internal::Map map) : history_ {std::move(map)}
    {}

    bool apply_event(const Event event) override
    {
        auto& map = internal::current(history_);
        switch (event)
        {
            case Event::MoveUp: return grid_rules::try_move_player(map, 0, -1);
            case Event::MoveLeft: return grid_rules::try_move_player(map, -1, 0);
            case Event::MoveDown: return grid_rules::try_move_player(map, 0, 1);
            case Event::MoveRight: return grid_rules::try_move_player(map, 1, 0);
            case Event::Commit: return grid_rules::map_commit(history_);
            case Event::Undo: return grid_rules::map_undo(history_);
        }

        return false;
    }

    bool solved() const override
    {
        return solution_rules::solved_equation(internal::current(history_));
    }

    MapView view() const override
    {
        return build_view(internal::current(history_));
    }

    std::string to_json() const override
    {
        return map_io::map_to_json(internal::current(history_));
    }

private:
    static MapView build_view(const internal::Map& map)
    {
        MapView view;
        view.width = internal::grid_width(map);
        view.height = internal::grid_height(map);
        view.commits_left = map.commits_left;
        view.undos_left = map.undos_left;
        view.cells.reserve(static_cast<size_t>(view.width * view.height));

        for (int y = 0; y < view.height; ++y)
        {
            for (int x = 0; x < view.width; ++x)
            {
                const auto props = map.grid[static_cast<size_t>(y)][static_cast<size_t>(x)].properties();
                CellView cell {.symbol = props.glyph, .properties = Object {}};
                if (props.is_empty) cell.properties = Empty {};
                else if (props.is_player) cell.properties = Player {};
                view.cells.push_back(cell);
            }
        }

        return view;
    }

    internal::History history_;
};

}  // namespace

std::unique_ptr<Game> Game::create_default()
{
    return std::make_unique<DefaultGame>(internal::make_map());
}

std::unique_ptr<Game> Game::from_json(const std::string_view json_text)
{
    return std::make_unique<DefaultGame>(map_io::map_from_json(json_text));
}
}  // namespace core
