#include "core/game.hpp"

#include "game_model.hpp"
#include "grid_rules.hpp"
#include "solution_rules.hpp"
#include "world_io.hpp"

#include <memory>
#include <string>

namespace core
{
namespace
{
class DefaultGame final : public Game
{
public:
    explicit DefaultGame(internal::World world) : history_ {std::move(world)}
    {}

    bool apply_event(const Event event) override
    {
        auto& world = internal::current(history_);
        switch (event)
        {
            case Event::MoveUp: return grid_rules::try_move_player(world, 0, -1);
            case Event::MoveLeft: return grid_rules::try_move_player(world, -1, 0);
            case Event::MoveDown: return grid_rules::try_move_player(world, 0, 1);
            case Event::MoveRight: return grid_rules::try_move_player(world, 1, 0);
            case Event::Commit: return grid_rules::world_commit(history_);
            case Event::Undo: return grid_rules::world_undo(history_);
        }

        return false;
    }

    bool solved() const override
    {
        return solution_rules::solved_equation(internal::current(history_));
    }

    std::string render() const override
    {
        return render_world(internal::current(history_));
    }

    std::string to_json() const override
    {
        return world_io::world_to_json(internal::current(history_));
    }

private:
    static std::string render_world(const internal::World& world)
    {
        std::string out;
        out += "\nWorld commits/undos: " + std::to_string(world.commits_left) + "/" + std::to_string(world.undos_left) + "\n\n";

        for (int y = 0; y < internal::grid_height(world); ++y)
        {
            for (int x = 0; x < internal::grid_width(world); ++x)
            {
                out.push_back(world.grid[y][x].properties().glyph);
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
    return std::make_unique<DefaultGame>(internal::make_world());
}

std::unique_ptr<Game> Game::from_json(const std::string_view json_text)
{
    return std::make_unique<DefaultGame>(world_io::world_from_json(json_text));
}
}  // namespace core
