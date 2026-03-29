#include "core/game.hpp"
#include "core/login.hpp"
#include "core/map_builder.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
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

class CurrentPathGuard
{
public:
    explicit CurrentPathGuard(const std::filesystem::path& new_path)
        : original_(std::filesystem::current_path())
    {
        std::filesystem::current_path(new_path);
    }

    ~CurrentPathGuard()
    {
        std::filesystem::current_path(original_);
    }

private:
    std::filesystem::path original_;
};

std::filesystem::path make_login_test_root()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "type_erasure_login_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "maps");
    std::filesystem::create_directories(root / "users");
    return root;
}

void write_text_file(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream output(path);
    ASSERT_TRUE(static_cast<bool>(output));
    output << text;
    ASSERT_TRUE(static_cast<bool>(output));
}

std::unique_ptr<core::Game> create_reference_game()
{
    return core::Game::from_json(R"({
  "version": 1,
  "size": { "width": 9, "height": 7 },
  "resources": { "commits": 6, "undos": 6 },
  "tiles": [
    { "x": 0, "y": 6, "symbol": "v" },
    { "x": 2, "y": 1, "symbol": "+", "pushable": true },
    { "x": 4, "y": 1, "symbol": "=", "pushable": true },
    { "x": 4, "y": 4, "symbol": "1", "pushable": true },
    { "x": 7, "y": 5, "symbol": "2", "pushable": true },
    { "x": 6, "y": 3, "symbol": "3", "pushable": true },
    { "x": 2, "y": 5, "symbol": "4", "pushable": true }
  ]
})");
}
}  // namespace

TEST(CoreGameTest, GivenNewGameWhenRenderingThenShowsInitialCounters)
{
    // Given
    std::unique_ptr<core::Game> game = create_reference_game();

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
    std::unique_ptr<core::Game> game = create_reference_game();

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
    std::unique_ptr<core::Game> game = create_reference_game();

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
    std::unique_ptr<core::Game> game = create_reference_game();

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

TEST(LoginViewTest, GivenNewUserWhenCreatingThenUserFileAndHighscoreAreCreated)
{
    // Given
    const std::filesystem::path root = make_login_test_root();
    CurrentPathGuard current_path(root);
    std::unique_ptr<core::LoginView> login = core::LoginView::create();

    // When
    core::User& user = login->create_user("alice");
    const auto& highscores = login->highscore_list();

    // Then
    EXPECT_EQ(user.username(), "alice");
    EXPECT_TRUE(user.solved_maps().empty());
    ASSERT_EQ(highscores.size(), 1U);
    EXPECT_EQ(highscores[0].username, "alice");
    EXPECT_EQ(highscores[0].solved_maps, 0);
    EXPECT_TRUE(std::filesystem::exists(root / "users" / "alice.json"));
}

TEST(LoginViewTest, GivenExistingUserFileWhenLoggingInThenSolvedMapsAreLoaded)
{
    // Given
    const std::filesystem::path root = make_login_test_root();
    CurrentPathGuard current_path(root);
    write_text_file(root / "users" / "bob.json", R"({
  "username": "bob",
  "solved_maps": ["assignment1", "zero"]
})");
    std::unique_ptr<core::LoginView> login = core::LoginView::create();

    // When
    core::User& user = login->login_as_user("bob");

    // Then
    ASSERT_EQ(user.solved_maps().size(), 2U);
    EXPECT_EQ(user.solved_maps()[0].map_id, "assignment1");
    EXPECT_EQ(user.solved_maps()[1].map_id, "zero");
}

TEST(LoginViewTest, GivenMapsDirectoryWhenSelectingMapThenGameLoadsFromSelectedMap)
{
    // Given
    const std::filesystem::path root = make_login_test_root();
    CurrentPathGuard current_path(root);
    write_text_file(root / "maps" / "zero.json", R"({
  "version": 1,
  "size": { "width": 3, "height": 1 },
  "tiles": [
    { "x": 0, "y": 0, "symbol": ">" },
    { "x": 1, "y": 0, "symbol": "=" },
    { "x": 2, "y": 0, "symbol": "0", "pushable": true }
  ]
})");
    std::unique_ptr<core::LoginView> login = core::LoginView::create();
    core::User& user = login->create_user("carol");

    // When
    const auto& maps = user.available_maps();
    std::unique_ptr<core::Game> game = user.select_map("zero");

    // Then
    ASSERT_EQ(maps.size(), 1U);
    EXPECT_EQ(maps[0].map_id, "zero");
    EXPECT_EQ(maps[0].display_name, "zero");
    EXPECT_EQ(extract_grid(game->view()), std::vector<std::string>({">=0"}));
}

TEST(LoginViewTest, GivenSelectedMapWhenGameBecomesSolvedThenSolvedMapIsPersistedToUserFile)
{
    // Given
    const std::filesystem::path root = make_login_test_root();
    CurrentPathGuard current_path(root);
    write_text_file(root / "maps" / "win.json", R"({
  "version": 1,
  "size": { "width": 5, "height": 1 },
  "tiles": [
    { "x": 0, "y": 0, "symbol": ">" },
    { "x": 1, "y": 0, "symbol": "1", "pushable": true },
    { "x": 2, "y": 0, "symbol": "+" },
    { "x": 3, "y": 0, "symbol": "=" },
    { "x": 4, "y": 0, "symbol": "1", "pushable": true }
  ]
})");
    std::unique_ptr<core::LoginView> login = core::LoginView::create();
    core::User& user = login->create_user("dana");
    std::unique_ptr<core::Game> game = user.select_map("win");

    // When
    ASSERT_TRUE(game->apply_event(core::Event::MoveRight));
    const std::string user_json = std::filesystem::exists(root / "users" / "dana.json")
                                      ? [&]() {
                                            std::ifstream input(root / "users" / "dana.json");
                                            return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
                                        }()
                                      : "";

    // Then
    EXPECT_TRUE(game->solved());
    ASSERT_EQ(user.solved_maps().size(), 1U);
    EXPECT_EQ(user.solved_maps()[0].map_id, "win");
    EXPECT_NE(user_json.find("\"win\""), std::string::npos);
    EXPECT_EQ(login->highscore_list()[0].solved_maps, 1);
}
