#include "core/game.hpp"

#include "game_model.hpp"
#include "grid_rules.hpp"
#include "solution_rules.hpp"

#include <memory>
#include <string>

namespace core
{
namespace
{
std::string render_world(const internal::World& world)
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
}  // namespace

struct Game::Impl
{
    internal::History history {internal::make_world()};
};

Game::Game() : impl_(std::make_unique<Impl>())
{}

Game::Game(const Game& other) : impl_(std::make_unique<Impl>(*other.impl_))
{}

Game& Game::operator=(const Game& other)
{
    if (this == &other) return *this;
    *impl_ = *other.impl_;
    return *this;
}

Game::Game(Game&& other) noexcept = default;

Game& Game::operator=(Game&& other) noexcept = default;

Game::~Game() = default;

bool Game::apply_event(const Event event)
{
    auto& world = internal::current(impl_->history);
    switch (event)
    {
        case Event::MoveUp: return grid_rules::try_move_player(world, 0, -1);
        case Event::MoveLeft: return grid_rules::try_move_player(world, -1, 0);
        case Event::MoveDown: return grid_rules::try_move_player(world, 0, 1);
        case Event::MoveRight: return grid_rules::try_move_player(world, 1, 0);
        case Event::Commit: return grid_rules::world_commit(impl_->history);
        case Event::Undo: return grid_rules::world_undo(impl_->history);
    }

    return false;
}

bool Game::solved() const
{
    return solution_rules::solved_equation(internal::current(impl_->history));
}

std::string Game::render() const
{
    return render_world(internal::current(impl_->history));
}
}  // namespace core
