#include "core/game.hpp"

#include <cassert>
#include <sstream>
#include <string>
#include <vector>

namespace
{
std::vector<std::string> extract_grid(const std::string& rendered)
{
    std::istringstream in(rendered);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(in, line))
    {
        lines.push_back(line);
    }

    std::vector<std::string> grid;
    for (const std::string& candidate : lines)
    {
        if (candidate.size() == 18) grid.push_back(candidate);
        if (grid.size() == 7) break;
    }

    std::vector<std::string> compact;
    compact.reserve(grid.size());
    for (const auto& row : grid)
    {
        std::string out;
        out.reserve(9);
        for (size_t i = 0; i < row.size(); i += 2)
        {
            out.push_back(row[i]);
        }
        compact.push_back(out);
    }
    return compact;
}
}  // namespace

int main()
{
    {
        core::Game game;
        const std::string rendered = game.render();
        assert(rendered.find("World commits/undos: 6/6") != std::string::npos);
        assert(!game.solved());
    }

    {
        core::Game game;
        assert(game.apply_event(core::Event::MoveRight));
        const auto grid = extract_grid(game.render());
        assert(grid.size() == 7);
        assert(grid[6][0] == ' ');
        assert(grid[6][1] == '>');
    }

    {
        core::Game game;
        assert(game.apply_event(core::Event::Commit));
        assert(game.render().find("World commits/undos: 5/6") != std::string::npos);
        assert(game.apply_event(core::Event::Undo));
        assert(game.render().find("World commits/undos: 6/5") != std::string::npos);
    }

    {
        core::Game game;
        assert(game.apply_event(core::Event::MoveUp));
        assert(game.apply_event(core::Event::MoveRight));
        assert(game.apply_event(core::Event::MoveRight));
        const auto grid = extract_grid(game.render());
        assert(grid[5][2] == '>');
        assert(grid[5][3] == '4');
    }

    return 0;
}
