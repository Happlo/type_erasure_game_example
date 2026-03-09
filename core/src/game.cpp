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
std::string map_to_grid_text(const internal::Map& map)
{
    std::string out;
    const int width = internal::grid_width(map);
    const int height = internal::grid_height(map);
    out.reserve(static_cast<size_t>(height * (width + 1)));

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            out.push_back(map.grid[static_cast<size_t>(y)][static_cast<size_t>(x)].view().symbol);
        }
        if (y + 1 < height) out.push_back('\n');
    }

    return out;
}

class DefaultGame final : public Game
{
public:
    explicit DefaultGame(internal::Map map) : history_ {std::move(map)}
    {}

    bool apply_event(const Event event) override
    {
        auto& map = internal::current(history_);
        const bool applied = [&]()
        {
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
        }();

        solved_ = solution_rules::solved_equation(map_to_grid_text(internal::current(history_)));
        return applied;
    }

    bool solved() const override
    {
        return solved_;
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
                view.cells.push_back(map.grid[static_cast<size_t>(y)][static_cast<size_t>(x)].view());
            }
        }

        return view;
    }

    internal::History history_;
    bool solved_ {solution_rules::solved_equation(map_to_grid_text(internal::current(history_)))};
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
