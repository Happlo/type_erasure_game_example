#include "core/game.hpp"

#include "game_model.hpp"
#include "grid_rules.hpp"
#include "solution_rules.hpp"
#include "map_io.hpp"

#include <memory>
#include <string>

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

    std::string render() const override
    {
        return render_map(internal::current(history_));
    }

    std::string to_json() const override
    {
        return map_io::map_to_json(internal::current(history_));
    }

private:
    static std::string render_map(const internal::Map& map)
    {
        std::string out;
        out += "\nMap commits/undos: " + std::to_string(map.commits_left) + "/" + std::to_string(map.undos_left) + "\n\n";

        for (int y = 0; y < internal::grid_height(map); ++y)
        {
            for (int x = 0; x < internal::grid_width(map); ++x)
            {
                out.push_back(map.grid[y][x].properties().glyph);
                out.push_back(' ');
            }
            out.push_back('\n');
        }

        out += "\nCommands: w/a/s/d, commit, undo, help, quit\n";
        out += "Goal: make the row containing '=' form a true equation.\n";
        out += "The '+' and '=' tiles stay fixed in place.\n";
        return out;
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
