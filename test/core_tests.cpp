#include "core/game.hpp"
#include "core/map_builder.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace
{
char view_glyph(const core::CellView& cell)
{
    return core::symbol_of(cell);
}

std::vector<std::string> extract_grid(const core::MapView& view)
{
    std::vector<std::string> compact;
    compact.reserve(static_cast<size_t>(view.height));

    for (int y = 0; y < view.height; ++y)
    {
        std::string row;
        row.reserve(static_cast<size_t>(view.width));
        for (int x = 0; x < view.width; ++x)
        {
            row.push_back(view_glyph(view.at(x, y)));
        }
        compact.push_back(std::move(row));
    }

    return compact;
}
}  // namespace

TEST(CoreGameTest, GivenNewGameWhenRenderingThenShowsInitialCounters)
{
    // Given
    std::unique_ptr<core::Game> game = core::Game::create_default();

    // When
    const core::MapView view = game->view();

    // Then
    EXPECT_EQ(view.commits_left, 6);
    EXPECT_EQ(view.undos_left, 6);
    EXPECT_FALSE(game->solved());
}

TEST(CoreGameTest, GivenPlayerAtStartWhenMoveRightThenMoveIsLegalAndFacingUpdates)
{
    // Given
    std::unique_ptr<core::Game> game = core::Game::create_default();

    // When
    const bool moved = game->apply_event(core::Event::MoveRight);
    const auto grid = extract_grid(game->view());

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
    const core::MapView after_commit = game->view();
    const bool undone = game->apply_event(core::Event::Undo);
    const core::MapView after_undo = game->view();

    // Then
    EXPECT_TRUE(committed);
    EXPECT_EQ(after_commit.commits_left, 5);
    EXPECT_EQ(after_commit.undos_left, 6);
    EXPECT_TRUE(undone);
    EXPECT_EQ(after_undo.commits_left, 6);
    EXPECT_EQ(after_undo.undos_left, 5);
}

TEST(CoreGameTest, GivenPushableTileWhenMovingIntoItThenPushMoveIsLegal)
{
    // Given
    std::unique_ptr<core::Game> game = core::Game::create_default();

    // When
    EXPECT_TRUE(game->apply_event(core::Event::MoveUp));
    EXPECT_TRUE(game->apply_event(core::Event::MoveRight));
    const bool pushed = game->apply_event(core::Event::MoveRight);
    const auto grid = extract_grid(game->view());

    // Then
    EXPECT_TRUE(pushed);
    ASSERT_EQ(grid.size(), 7U);
    EXPECT_EQ(grid[5][2], '>');
    EXPECT_EQ(grid[5][3], '4');
}

TEST(CoreGameTest, GivenPickableItemInFrontWhenPickingAndDroppingThenInventoryPersistsAcrossMove)
{
    // Given
    std::unique_ptr<core::Game> game = core::Game::from_json(R"({
  "version": 1,
  "size": { "width": 4, "height": 3 },
  "resources": { "commits": 2, "undos": 1 },
  "tiles": [
    { "x": 1, "y": 1, "symbol": ">", "pushable": false },
    { "x": 2, "y": 1, "symbol": "*", "pickable": true }
  ]
})");

    // When
    const bool picked = game->apply_event(core::Event::PickItem);
    const auto after_pick = extract_grid(game->view());
    const bool moved = game->apply_event(core::Event::MoveRight);
    const auto after_move = extract_grid(game->view());
    const bool dropped = game->apply_event(core::Event::DropItem);
    const auto after_drop = extract_grid(game->view());

    // Then
    EXPECT_TRUE(picked);
    EXPECT_TRUE(moved);
    EXPECT_TRUE(dropped);
    EXPECT_EQ(after_pick[1][2], ' ');
    EXPECT_EQ(after_move[1][1], ' ');
    EXPECT_EQ(after_move[1][2], '>');
    EXPECT_EQ(after_drop[1][2], '>');
    EXPECT_EQ(after_drop[1][3], '*');
}

TEST(CoreGameTest, GivenPickedItemsWhenViewingMapThenPlayerInventoryIsIncludedInView)
{
    // Given
    std::unique_ptr<core::MapBuilder> map = core::MapBuilder::create(5, 2);
    map->apply_brush(0, 0, core::Brush{.symbol = '>'});
    map->apply_brush(1, 0, core::Brush{.symbol = '*', .pickable = true});
    map->apply_brush(2, 0, core::Brush{.symbol = '!', .pickable = true});
    std::unique_ptr<core::Game> game = core::Game::from_json(map->to_json());

    // When
    ASSERT_TRUE(game->apply_event(core::Event::PickItem));
    ASSERT_TRUE(game->apply_event(core::Event::MoveRight));
    ASSERT_TRUE(game->apply_event(core::Event::PickItem));
    const core::MapView view = game->view();

    // Then
    ASSERT_TRUE(view.player.has_value());
    ASSERT_EQ(view.player->inventory.size(), 2U);
    EXPECT_EQ(view.player->inventory[0].symbol, '*');
    EXPECT_EQ(view.player->inventory[1].symbol, '!');
}

TEST(CoreGameTest, GivenNonPickableOrBlockedFrontCellWhenPickingOrDroppingThenActionFails)
{
    // Given
    std::unique_ptr<core::Game> game = core::Game::from_json(R"({
  "version": 1,
  "size": { "width": 4, "height": 2 },
  "tiles": [
    { "x": 0, "y": 0, "symbol": ">" },
    { "x": 1, "y": 0, "symbol": "+", "pushable": true },
    { "x": 2, "y": 0, "symbol": "*", "pickable": true }
  ]
})");

    // When / Then
    EXPECT_FALSE(game->apply_event(core::Event::PickItem));
    EXPECT_FALSE(game->apply_event(core::Event::DropItem));
}

TEST(CoreGameTest, GivenCustomMapJsonWhenLoadingThenMapIsBuiltFromJson)
{
    // Given
    const std::string json_map = R"({
  "version": 1,
  "size": { "width": 5, "height": 3 },
  "resources": { "commits": 2, "undos": 1 },
  "tiles": [
    { "x": 0, "y": 2, "symbol": ">", "pushable": false },
    { "x": 1, "y": 2, "symbol": "4", "pushable": true },
    { "x": 2, "y": 1, "symbol": "+", "pushable": true },
    { "x": 3, "y": 1, "symbol": "=", "pushable": true }
  ]
})";

    // When
    std::unique_ptr<core::Game> game = core::Game::from_json(json_map);
    const core::MapView view = game->view();
    const auto grid = extract_grid(view);

    // Then
    EXPECT_EQ(view.commits_left, 2);
    EXPECT_EQ(view.undos_left, 1);
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
    { "x": 0, "y": 2, "symbol": "^" },
    { "x": 1, "y": 2, "symbol": "2", "pushable": true },
    { "x": 1, "y": 1, "symbol": "*" }
  ]
})");

    // When
    const std::string encoded = original->to_json();
    std::unique_ptr<core::Game> restored = core::Game::from_json(encoded);

    // Then
    EXPECT_EQ(extract_grid(restored->view()), extract_grid(original->view()));
    EXPECT_EQ(restored->to_json(), encoded);
}
