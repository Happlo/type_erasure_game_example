#include "core/game.hpp"

#include <gtest/gtest.h>

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

TEST(CoreGameTest, GivenNewGameWhenRenderingThenShowsInitialCounters)
{
    // Given
    core::Game game;

    // When
    const std::string rendered = game.render();

    // Then
    EXPECT_NE(rendered.find("World commits/undos: 6/6"), std::string::npos);
    EXPECT_FALSE(game.solved());
}

TEST(CoreGameTest, GivenPlayerAtStartWhenMoveRightThenMoveIsLegalAndFacingUpdates)
{
    // Given
    core::Game game;

    // When
    const bool moved = game.apply_event(core::Event::MoveRight);
    const auto grid = extract_grid(game.render());

    // Then
    EXPECT_TRUE(moved);
    ASSERT_EQ(grid.size(), 7U);
    EXPECT_EQ(grid[6][0], ' ');
    EXPECT_EQ(grid[6][1], '>');
}

TEST(CoreGameTest, GivenCommittedSnapshotWhenUndoThenUndoCounterDecrements)
{
    // Given
    core::Game game;

    // When
    const bool committed = game.apply_event(core::Event::Commit);
    const std::string after_commit = game.render();
    const bool undone = game.apply_event(core::Event::Undo);
    const std::string after_undo = game.render();

    // Then
    EXPECT_TRUE(committed);
    EXPECT_NE(after_commit.find("World commits/undos: 5/6"), std::string::npos);
    EXPECT_TRUE(undone);
    EXPECT_NE(after_undo.find("World commits/undos: 6/5"), std::string::npos);
}

TEST(CoreGameTest, GivenPushableTileWhenMovingIntoItThenPushMoveIsLegal)
{
    // Given
    core::Game game;

    // When
    EXPECT_TRUE(game.apply_event(core::Event::MoveUp));
    EXPECT_TRUE(game.apply_event(core::Event::MoveRight));
    const bool pushed = game.apply_event(core::Event::MoveRight);
    const auto grid = extract_grid(game.render());

    // Then
    EXPECT_TRUE(pushed);
    ASSERT_EQ(grid.size(), 7U);
    EXPECT_EQ(grid[5][2], '>');
    EXPECT_EQ(grid[5][3], '4');
}
