#include "core/game.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
std::vector<std::string> extract_grid(const std::string& rendered)
{
    std::istringstream in(rendered);
    std::string line;
    std::vector<std::string> compact;
    while (std::getline(in, line))
    {
        if (line.empty() || line.size() % 2 != 0) continue;
        bool is_grid_row = true;
        for (size_t i = 1; i < line.size(); i += 2)
        {
            if (line[i] != ' ')
            {
                is_grid_row = false;
                break;
            }
        }

        if (!is_grid_row) continue;

        std::string row;
        row.reserve(line.size() / 2);
        for (size_t i = 0; i < line.size(); i += 2)
        {
            row.push_back(line[i]);
        }
        compact.push_back(row);
    }
    return compact;
}
}  // namespace

TEST(CoreGameTest, GivenNewGameWhenRenderingThenShowsInitialCounters)
{
    // Given
    std::unique_ptr<core::Game> game = core::Game::create_default();

    // When
    const std::string rendered = game->render();

    // Then
    EXPECT_NE(rendered.find("Map commits/undos: 6/6"), std::string::npos);
    EXPECT_FALSE(game->solved());
}

TEST(CoreGameTest, GivenPlayerAtStartWhenMoveRightThenMoveIsLegalAndFacingUpdates)
{
    // Given
    std::unique_ptr<core::Game> game = core::Game::create_default();

    // When
    const bool moved = game->apply_event(core::Event::MoveRight);
    const auto grid = extract_grid(game->render());

    // Then
    EXPECT_TRUE(moved);
    ASSERT_EQ(grid.size(), 7U);
    EXPECT_EQ(grid[6][0], ' ');
    EXPECT_EQ(grid[6][1], '>');
}

TEST(CoreGameTest, GivenCommittedSnapshotWhenUndoThenUndoCounterDecrements)
{
    // Given
    std::unique_ptr<core::Game> game = core::Game::create_default();

    // When
    const bool committed = game->apply_event(core::Event::Commit);
    const std::string after_commit = game->render();
    const bool undone = game->apply_event(core::Event::Undo);
    const std::string after_undo = game->render();

    // Then
    EXPECT_TRUE(committed);
    EXPECT_NE(after_commit.find("Map commits/undos: 5/6"), std::string::npos);
    EXPECT_TRUE(undone);
    EXPECT_NE(after_undo.find("Map commits/undos: 6/5"), std::string::npos);
}

TEST(CoreGameTest, GivenPushableTileWhenMovingIntoItThenPushMoveIsLegal)
{
    // Given
    std::unique_ptr<core::Game> game = core::Game::create_default();

    // When
    EXPECT_TRUE(game->apply_event(core::Event::MoveUp));
    EXPECT_TRUE(game->apply_event(core::Event::MoveRight));
    const bool pushed = game->apply_event(core::Event::MoveRight);
    const auto grid = extract_grid(game->render());

    // Then
    EXPECT_TRUE(pushed);
    ASSERT_EQ(grid.size(), 7U);
    EXPECT_EQ(grid[5][2], '>');
    EXPECT_EQ(grid[5][3], '4');
}

TEST(CoreGameTest, GivenCustomMapJsonWhenLoadingThenMapIsBuiltFromJson)
{
    // Given
    const std::string json_map = R"({
  "version": 1,
  "size": { "width": 5, "height": 3 },
  "resources": { "commits": 2, "undos": 1 },
  "tiles": [
    { "x": 0, "y": 2, "kind": "player", "facing": "east" },
    { "x": 1, "y": 2, "kind": "number", "value": 4, "pushable": true },
    { "x": 2, "y": 1, "kind": "plus", "pushable": true },
    { "x": 3, "y": 1, "kind": "equals", "pushable": true }
  ]
})";

    // When
    std::unique_ptr<core::Game> game = core::Game::from_json(json_map);
    const auto grid = extract_grid(game->render());

    // Then
    EXPECT_NE(game->render().find("Map commits/undos: 2/1"), std::string::npos);
    ASSERT_EQ(grid.size(), 3U);
    EXPECT_EQ(grid[2][0], '>');
    EXPECT_EQ(grid[2][1], '4');
    EXPECT_EQ(grid[1][2], '+');
    EXPECT_EQ(grid[1][3], '=');
}

TEST(CoreGameTest, GivenGameWhenSerializingAndParsingThenRoundtripPreservesMap)
{
    // Given
    std::unique_ptr<core::Game> original = core::Game::from_json(R"({
  "version": 1,
  "size": { "width": 4, "height": 3 },
  "resources": { "commits": 3, "undos": 7 },
  "tiles": [
    { "x": 0, "y": 2, "kind": "player", "facing": "north" },
    { "x": 1, "y": 2, "kind": "number", "value": 2, "pushable": true, "blocking": true },
    { "x": 1, "y": 1, "kind": "symbol", "glyph": "*", "blocking": true }
  ]
})");

    // When
    const std::string encoded = original->to_json();
    std::unique_ptr<core::Game> restored = core::Game::from_json(encoded);

    // Then
    EXPECT_EQ(restored->render(), original->render());
    EXPECT_EQ(restored->to_json(), encoded);
}
