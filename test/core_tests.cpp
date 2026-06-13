#include "core/game.hpp"
#include "core/login.hpp"
#include "core/map_builder.hpp"
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace core
{
bool operator==(const CellView &cell, const char symbol) { return symbol_of(cell) == symbol; }

bool operator==(const char symbol, const CellView &cell) { return cell == symbol; }
} // namespace core

namespace
{
using ManipulationLevel = core::Object::ManipulationLevel;

class CurrentPathGuard
{
  public:
    explicit CurrentPathGuard(const std::filesystem::path &new_path)
        : original_(std::filesystem::current_path())
    {
        std::filesystem::current_path(new_path);
    }

    ~CurrentPathGuard() { std::filesystem::current_path(original_); }

  private:
    std::filesystem::path original_;
};

std::filesystem::path make_login_test_root()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "type_erasure_login_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "maps");
    std::filesystem::create_directories(root / "users");
    return root;
}

void write_text_file(const std::filesystem::path &path, const std::string &text)
{
    std::ofstream output(path);
    ASSERT_TRUE(static_cast<bool>(output));
    output << text;
    ASSERT_TRUE(static_cast<bool>(output));
}

std::unique_ptr<core::Game> create_reference_game()
{
    std::unique_ptr<core::MapBuilder> map = core::MapBuilder::create(9, 7);
    map->set_commits_left(6)
        .set_undos_left(6)
        .apply_brush(0, 6, core::Brush{.symbol = 'v'})
        .apply_brush(2, 1,
                     core::Brush{.symbol = '+', .manipulation_level = ManipulationLevel::Push})
        .apply_brush(4, 1,
                     core::Brush{.symbol = '=', .manipulation_level = ManipulationLevel::Push})
        .apply_brush(4, 4,
                     core::Brush{.symbol = '1', .manipulation_level = ManipulationLevel::Push})
        .apply_brush(7, 5,
                     core::Brush{.symbol = '2', .manipulation_level = ManipulationLevel::Push})
        .apply_brush(6, 3,
                     core::Brush{.symbol = '3', .manipulation_level = ManipulationLevel::Push})
        .apply_brush(2, 5,
                     core::Brush{.symbol = '4', .manipulation_level = ManipulationLevel::Push});
    return map->create_game();
}
} // namespace

TEST(CoreGameTest, GivenPlayerAtStartWhenMoveRightThenMoveIsLegalAndFacingUpdates)
{
    // Given
    std::unique_ptr<core::Game> game = create_reference_game();
    ASSERT_TRUE(game->view().player.has_value());
    EXPECT_EQ(game->view().player->location, (core::Location{.x = 0, .y = 6}));
    EXPECT_EQ(game->view().player->symbol, 'v');
    EXPECT_EQ(game->view().at(0, 6), ' ');
    EXPECT_EQ(game->view().at(1, 6), ' ');

    // When
    game->apply_event(core::Event::MoveRight);

    // Then
    EXPECT_EQ(game->view().at(0, 6), ' ');
    ASSERT_TRUE(game->view().player.has_value());
    EXPECT_EQ(game->view().player->location, (core::Location{.x = 1, .y = 6}));
    EXPECT_EQ(game->view().player->symbol, '>');
    EXPECT_EQ(game->view().at(1, 6), ' ');
}

TEST(CoreGameTest, GivenCommittedSnapshotWhenUndoThenUndoCounterDecrements)
{
    // Given
    std::unique_ptr<core::Game> game = create_reference_game();
    EXPECT_EQ(game->view().commits_left, 6);
    game->apply_event(core::Event::Commit);
    EXPECT_EQ(game->view().commits_left, 5);
    EXPECT_EQ(game->view().undos_left, 6);

    // When
    game->apply_event(core::Event::Undo);

    // Then
    EXPECT_EQ(game->view().commits_left, 6);
    EXPECT_EQ(game->view().undos_left, 5);
}

TEST(CoreGameTest, GivenPushableTileWhenMovingIntoItThenPushMoveIsLegal)
{
    // Given
    std::unique_ptr<core::Game> game = create_reference_game();
    game->apply_event(core::Event::MoveUp);
    game->apply_event(core::Event::MoveRight);
    ASSERT_TRUE(game->view().player.has_value());
    EXPECT_EQ(game->view().player->location, (core::Location{.x = 1, .y = 5}));
    EXPECT_EQ(game->view().player->symbol, '>');
    EXPECT_EQ(game->view().at(2, 5), '4');

    // When
    game->apply_event(core::Event::MoveRight);

    // Then
    ASSERT_TRUE(game->view().player.has_value());
    EXPECT_EQ(game->view().player->location, (core::Location{.x = 2, .y = 5}));
    EXPECT_EQ(game->view().player->symbol, '>');
    EXPECT_EQ(game->view().at(2, 5), ' ');
    EXPECT_EQ(game->view().at(3, 5), '4');
}

TEST(CoreGameTest, pickable_item_can_be_picked_up)
{
    // Given
    std::unique_ptr<core::MapBuilder> map = core::MapBuilder::create(4, 3);
    map->set_commits_left(2)
        .set_undos_left(1)
        .apply_brush(1, 1, core::Brush{.symbol = '>'})
        .apply_brush(2, 1,
                     core::Brush{.symbol = '*', .manipulation_level = ManipulationLevel::Pick});
    std::unique_ptr<core::Game> game = map->create_game();

    // When
    game->apply_event(core::Event::PickItem);

    // Then
    EXPECT_EQ(game->view().at(2, 1), ' ');
    EXPECT_EQ(game->view().player->inventory.at(0).symbol, '*');
}

TEST(CoreGameTest, GivenPickableItemWhenWalkingIntoItThenItCanAlsoBePushed)
{
    // Given
    std::unique_ptr<core::MapBuilder> map = core::MapBuilder::create(4, 1);
    map->apply_brush(0, 0, core::Brush{.symbol = '>'})
        .apply_brush(1, 0,
                     core::Brush{.symbol = '*', .manipulation_level = ManipulationLevel::Pick});
    std::unique_ptr<core::Game> game = map->create_game();

    // When
    game->apply_event(core::Event::MoveRight);

    // Then
    EXPECT_EQ(game->view().at(0, 0), ' ');
    ASSERT_TRUE(game->view().player.has_value());
    EXPECT_EQ(game->view().player->location, (core::Location{.x = 1, .y = 0}));
    EXPECT_EQ(game->view().player->symbol, '>');
    EXPECT_EQ(game->view().at(1, 0), ' ');
    EXPECT_EQ(game->view().at(2, 0), '*');
    EXPECT_EQ(game->view().at(3, 0), ' ');
}

TEST(CoreGameTest, GivenPickedItemsWhenViewingMapThenPlayerInventoryIsIncludedInView)
{
    // Given
    std::unique_ptr<core::MapBuilder> map = core::MapBuilder::create(5, 2);
    map->apply_brush(0, 0, core::Brush{.symbol = '>'})
        .apply_brush(1, 0,
                     core::Brush{.symbol = '*', .manipulation_level = ManipulationLevel::Pick})
        .apply_brush(2, 0,
                     core::Brush{.symbol = '!', .manipulation_level = ManipulationLevel::Pick});
    std::unique_ptr<core::Game> game = map->create_game();

    // When
    game->apply_event(core::Event::PickItem);
    game->apply_event(core::Event::MoveRight);
    game->apply_event(core::Event::PickItem);
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
    std::unique_ptr<core::MapBuilder> map = core::MapBuilder::create(4, 2);
    map->apply_brush(0, 0, core::Brush{.symbol = '>'})
        .apply_brush(1, 0,
                     core::Brush{.symbol = '+', .manipulation_level = ManipulationLevel::Push})
        .apply_brush(2, 0,
                     core::Brush{.symbol = '*', .manipulation_level = ManipulationLevel::Pick});
    std::unique_ptr<core::Game> game = map->create_game();

    // When / Then
    game->apply_event(core::Event::PickItem);
    ASSERT_TRUE(game->view().player.has_value());
    EXPECT_EQ(game->view().player->location, (core::Location{.x = 0, .y = 0}));
    EXPECT_EQ(game->view().player->symbol, '>');
    EXPECT_EQ(game->view().at(0, 0), ' ');
    EXPECT_EQ(game->view().at(1, 0), '+');
    EXPECT_TRUE(game->view().player->inventory.empty());

    game->apply_event(core::Event::DropItem);
    ASSERT_TRUE(game->view().player.has_value());
    EXPECT_EQ(game->view().player->location, (core::Location{.x = 0, .y = 0}));
    EXPECT_EQ(game->view().player->symbol, '>');
    EXPECT_EQ(game->view().at(0, 0), ' ');
    EXPECT_EQ(game->view().at(1, 0), '+');
    EXPECT_TRUE(game->view().player->inventory.empty());
}

TEST(CoreGameTest, GivenMapBuilderWhenCreatingGameThenGameUsesBuilderMap)
{
    // Given
    std::unique_ptr<core::MapBuilder> map = core::MapBuilder::create(5, 3);
    map->set_commits_left(2)
        .set_undos_left(1)
        .apply_brush(0, 2, core::Brush{.symbol = '>'})
        .apply_brush(1, 2,
                     core::Brush{.symbol = '4', .manipulation_level = ManipulationLevel::Push})
        .apply_brush(2, 1,
                     core::Brush{.symbol = '+', .manipulation_level = ManipulationLevel::Push})
        .apply_brush(3, 1,
                     core::Brush{.symbol = '=', .manipulation_level = ManipulationLevel::Push});

    // When
    std::unique_ptr<core::Game> game = map->create_game();
    const core::MapView view = game->view();

    // Then
    EXPECT_EQ(view.commits_left, 2);
    EXPECT_EQ(view.undos_left, 1);
    ASSERT_TRUE(view.player.has_value());
    EXPECT_EQ(view.player->location, (core::Location{.x = 0, .y = 2}));
    EXPECT_EQ(view.player->symbol, '>');
    EXPECT_EQ(view.at(0, 2), ' ');
    EXPECT_EQ(view.at(1, 2), '4');
    EXPECT_EQ(view.at(2, 1), '+');
    EXPECT_EQ(view.at(3, 1), '=');
}

TEST(CoreGameTest, GivenMapBuilderWhenSavingAndLoadingThenMapIsPreserved)
{
    // Given
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "type_erasure_roundtrip_map.json";
    std::unique_ptr<core::MapBuilder> original = core::MapBuilder::create(4, 3);
    original->set_commits_left(3)
        .set_undos_left(7)
        .apply_brush(0, 2, core::Brush{.symbol = '^'})
        .apply_brush(1, 2,
                     core::Brush{.symbol = '2', .manipulation_level = ManipulationLevel::Push})
        .apply_brush(1, 1, core::Brush{.symbol = '*'});

    // When
    original->save_to_file(path);
    std::unique_ptr<core::MapBuilder> restored = core::MapBuilder::load_from_file(path);

    // Then
    ASSERT_TRUE(restored->view().player.has_value());
    EXPECT_EQ(restored->view().player->location, (core::Location{.x = 0, .y = 2}));
    EXPECT_EQ(restored->view().player->symbol, '^');
    EXPECT_EQ(restored->view().at(0, 2), ' ');
    EXPECT_EQ(restored->view().at(1, 2), '2');
    EXPECT_EQ(restored->view().at(1, 1), '*');
    EXPECT_EQ(restored->view().commits_left, original->view().commits_left);
    EXPECT_EQ(restored->view().undos_left, original->view().undos_left);
}

TEST(LoginViewTest, GivenNewUserWhenCreatingThenUserFileAndHighscoreAreCreated)
{
    // Given
    const std::filesystem::path root = make_login_test_root();
    CurrentPathGuard current_path(root);
    std::unique_ptr<core::LoginView> login = core::LoginView::create();

    // When
    core::User &user = login->create_user("alice");
    const auto &highscores = login->highscore_list();

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
    core::User &user = login->login_as_user("bob");

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
    std::unique_ptr<core::MapBuilder> map = core::MapBuilder::create(3, 1);
    map->apply_brush(0, 0, core::Brush{.symbol = '>'})
        .apply_brush(1, 0, core::Brush{.symbol = '='})
        .apply_brush(2, 0,
                     core::Brush{.symbol = '0', .manipulation_level = ManipulationLevel::Push});
    map->save_to_file(root / "maps" / "zero.json");
    std::unique_ptr<core::LoginView> login = core::LoginView::create();
    core::User &user = login->create_user("carol");

    // When
    const auto &maps = user.available_maps();
    std::unique_ptr<core::Game> game = user.select_map("zero");

    // Then
    ASSERT_EQ(maps.size(), 1U);
    EXPECT_EQ(maps[0].map_id, "zero");
    EXPECT_EQ(maps[0].display_name, "zero");
    ASSERT_TRUE(game->view().player.has_value());
    EXPECT_EQ(game->view().player->location, (core::Location{.x = 0, .y = 0}));
    EXPECT_EQ(game->view().player->symbol, '>');
    EXPECT_EQ(game->view().at(0, 0), ' ');
    EXPECT_EQ(game->view().at(1, 0), '=');
    EXPECT_EQ(game->view().at(2, 0), '0');
}

TEST(LoginViewTest, GivenUserCreatesAndSavesMapThenMapIsStoredUnderUserDirectory)
{
    // Given
    const std::filesystem::path root = make_login_test_root();
    CurrentPathGuard current_path(root);
    std::unique_ptr<core::LoginView> login = core::LoginView::create();
    core::User &user = login->create_user("eve");
    std::unique_ptr<core::MapBuilder> map = user.create_new_map();
    map->resize(3, 1)
        .apply_brush(0, 0, core::Brush{.symbol = '>'})
        .apply_brush(1, 0, core::Brush{.symbol = '='})
        .apply_brush(2, 0,
                     core::Brush{.symbol = '0', .manipulation_level = ManipulationLevel::Push});

    // When
    map->save_to_file("custom.json");
    std::unique_ptr<core::Game> game = user.select_map("eve/custom");

    // Then
    ASSERT_EQ(user.available_maps().size(), 1U);
    EXPECT_EQ(user.available_maps()[0].map_id, "eve/custom");
    EXPECT_EQ(user.available_maps()[0].display_name, "custom");
    EXPECT_TRUE(std::filesystem::exists(root / "maps" / "eve" / "custom.json"));
    ASSERT_TRUE(game->view().player.has_value());
    EXPECT_EQ(game->view().player->location, (core::Location{.x = 0, .y = 0}));
    EXPECT_EQ(game->view().player->symbol, '>');
    EXPECT_EQ(game->view().at(0, 0), ' ');
    EXPECT_EQ(game->view().at(1, 0), '=');
    EXPECT_EQ(game->view().at(2, 0), '0');
}

TEST(MapBuilderTest, GivenMapWithoutPlayerWhenLoadingIntoBuilderThenParsingSucceeds)
{
    // Given
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "type_erasure_no_player_map.json";
    std::unique_ptr<core::MapBuilder> map = core::MapBuilder::create(4, 3);
    map->set_commits_left(0)
        .set_undos_left(0)
        .clear_cell(0, 0)
        .apply_brush(1, 0, core::Brush{.symbol = '1'})
        .apply_brush(2, 0, core::Brush{.symbol = '+'})
        .apply_brush(3, 0, core::Brush{.symbol = '1'});
    map->save_to_file(path);

    // When
    std::unique_ptr<core::MapBuilder> builder = core::MapBuilder::load_from_file(path);

    // Then
    EXPECT_EQ(builder->view().width, 4);
    EXPECT_EQ(builder->view().height, 3);
    EXPECT_EQ(builder->view().at(1, 0), '1');
    EXPECT_EQ(builder->view().at(2, 0), '+');
}
