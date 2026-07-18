#include "core/game.hpp"
#include "core/login.hpp"
#include "core/map_builder.hpp"
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace
{
using ManipulationLevel = core::Object::ManipulationLevel;

char symbol_at(const core::MapView &view, const int x, const int y)
{
    const auto object = view.objects.find(core::Location{.x = x, .y = y});
    return object == view.objects.end() ? ' ' : object->second.symbol;
}

core::Location location(const int x, const int y) { return core::Location{.x = x, .y = y}; }

core::Object object(const char symbol,
                    const ManipulationLevel manipulation_level = ManipulationLevel::None)
{
    return core::Object{.symbol = symbol, .manipulation_level = manipulation_level};
}

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
    std::unique_ptr<core::MapBuilder> map = core::MapBuilder::create();
    map->set_commits_left(6)
        .set_undos_left(6)
        .add_object(location(0, 6), object('v'))
        .add_object(location(2, 1), object('+', ManipulationLevel::Push))
        .add_object(location(4, 1), object('=', ManipulationLevel::Push))
        .add_object(location(4, 4), object('1', ManipulationLevel::Push))
        .add_object(location(7, 5), object('2', ManipulationLevel::Push))
        .add_object(location(6, 3), object('3', ManipulationLevel::Push))
        .add_object(location(2, 5), object('4', ManipulationLevel::Push));
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
    EXPECT_EQ(symbol_at(game->view(), 0, 6), ' ');
    EXPECT_EQ(symbol_at(game->view(), 1, 6), ' ');

    // When
    game->apply_event(core::Event::MoveRight);

    // Then
    EXPECT_EQ(symbol_at(game->view(), 0, 6), ' ');
    ASSERT_TRUE(game->view().player.has_value());
    EXPECT_EQ(game->view().player->location, (core::Location{.x = 1, .y = 6}));
    EXPECT_EQ(game->view().player->symbol, '>');
    EXPECT_EQ(symbol_at(game->view(), 1, 6), ' ');
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
    EXPECT_EQ(symbol_at(game->view(), 2, 5), '4');

    // When
    game->apply_event(core::Event::MoveRight);

    // Then
    ASSERT_TRUE(game->view().player.has_value());
    EXPECT_EQ(game->view().player->location, (core::Location{.x = 2, .y = 5}));
    EXPECT_EQ(game->view().player->symbol, '>');
    EXPECT_EQ(symbol_at(game->view(), 2, 5), ' ');
    EXPECT_EQ(symbol_at(game->view(), 3, 5), '4');
}

TEST(CoreGameTest, GivenSeveralPushableTilesInARowWhenMovingIntoThemThenAllArePushed)
{
    // Given
    std::unique_ptr<core::MapBuilder> map = core::MapBuilder::create();
    map->add_object(location(0, 0), object('>'))
        .add_object(location(1, 0), object('1', ManipulationLevel::Push))
        .add_object(location(2, 0), object('+', ManipulationLevel::Push))
        .add_object(location(3, 0), object('2', ManipulationLevel::Push));
    std::unique_ptr<core::Game> game = map->create_game();

    // When
    game->apply_event(core::Event::MoveRight);

    // Then
    ASSERT_TRUE(game->view().player.has_value());
    EXPECT_EQ(game->view().player->location, location(1, 0));
    EXPECT_EQ(symbol_at(game->view(), 1, 0), ' ');
    EXPECT_EQ(symbol_at(game->view(), 2, 0), '1');
    EXPECT_EQ(symbol_at(game->view(), 3, 0), '+');
    EXPECT_EQ(symbol_at(game->view(), 4, 0), '2');
}

TEST(CoreGameTest, GivenPushableTilesBlockedByFixedTileWhenMovingThenNothingMoves)
{
    // Given
    std::unique_ptr<core::MapBuilder> map = core::MapBuilder::create();
    map->add_object(location(0, 0), object('>'))
        .add_object(location(1, 0), object('1', ManipulationLevel::Push))
        .add_object(location(2, 0), object('+', ManipulationLevel::Push))
        .add_object(location(3, 0), object('#'));
    std::unique_ptr<core::Game> game = map->create_game();

    // When
    game->apply_event(core::Event::MoveRight);

    // Then
    ASSERT_TRUE(game->view().player.has_value());
    EXPECT_EQ(game->view().player->location, location(0, 0));
    EXPECT_EQ(symbol_at(game->view(), 1, 0), '1');
    EXPECT_EQ(symbol_at(game->view(), 2, 0), '+');
    EXPECT_EQ(symbol_at(game->view(), 3, 0), '#');
}

TEST(CoreGameTest, pickable_item_can_be_picked_up)
{
    // Given
    std::unique_ptr<core::MapBuilder> map = core::MapBuilder::create();
    map->set_commits_left(2)
        .set_undos_left(1)
        .add_object(location(1, 1), object('>'))
        .add_object(location(2, 1), object('*', ManipulationLevel::Pick));
    std::unique_ptr<core::Game> game = map->create_game();

    // When
    game->apply_event(core::Event::PickItem);

    // Then
    EXPECT_EQ(symbol_at(game->view(), 2, 1), ' ');
    EXPECT_EQ(game->view().player->inventory.at(0).symbol, '*');
}

TEST(CoreGameTest, GivenPickableItemWhenWalkingIntoItThenItCanAlsoBePushed)
{
    // Given
    std::unique_ptr<core::MapBuilder> map = core::MapBuilder::create();
    map->add_object(location(0, 0), object('>'))
        .add_object(location(1, 0), object('*', ManipulationLevel::Pick));
    std::unique_ptr<core::Game> game = map->create_game();

    // When
    game->apply_event(core::Event::MoveRight);

    // Then
    EXPECT_EQ(symbol_at(game->view(), 0, 0), ' ');
    ASSERT_TRUE(game->view().player.has_value());
    EXPECT_EQ(game->view().player->location, (core::Location{.x = 1, .y = 0}));
    EXPECT_EQ(game->view().player->symbol, '>');
    EXPECT_EQ(symbol_at(game->view(), 1, 0), ' ');
    EXPECT_EQ(symbol_at(game->view(), 2, 0), '*');
    EXPECT_EQ(symbol_at(game->view(), 3, 0), ' ');
}

TEST(CoreGameTest, GivenPickedItemsWhenViewingMapThenPlayerInventoryIsIncludedInView)
{
    // Given
    std::unique_ptr<core::MapBuilder> map = core::MapBuilder::create();
    map->add_object(location(0, 0), object('>'))
        .add_object(location(1, 0), object('*', ManipulationLevel::Pick))
        .add_object(location(2, 0), object('!', ManipulationLevel::Pick));
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
    std::unique_ptr<core::MapBuilder> map = core::MapBuilder::create();
    map->add_object(location(0, 0), object('>'))
        .add_object(location(1, 0), object('+', ManipulationLevel::Push))
        .add_object(location(2, 0), object('*', ManipulationLevel::Pick));
    std::unique_ptr<core::Game> game = map->create_game();

    // When / Then
    game->apply_event(core::Event::PickItem);
    ASSERT_TRUE(game->view().player.has_value());
    EXPECT_EQ(game->view().player->location, (core::Location{.x = 0, .y = 0}));
    EXPECT_EQ(game->view().player->symbol, '>');
    EXPECT_EQ(symbol_at(game->view(), 0, 0), ' ');
    EXPECT_EQ(symbol_at(game->view(), 1, 0), '+');
    EXPECT_TRUE(game->view().player->inventory.empty());

    game->apply_event(core::Event::DropItem);
    ASSERT_TRUE(game->view().player.has_value());
    EXPECT_EQ(game->view().player->location, (core::Location{.x = 0, .y = 0}));
    EXPECT_EQ(game->view().player->symbol, '>');
    EXPECT_EQ(symbol_at(game->view(), 0, 0), ' ');
    EXPECT_EQ(symbol_at(game->view(), 1, 0), '+');
    EXPECT_TRUE(game->view().player->inventory.empty());
}

TEST(CoreGameTest, GivenMapBuilderWhenCreatingGameThenGameUsesBuilderMap)
{
    // Given
    std::unique_ptr<core::MapBuilder> map = core::MapBuilder::create();
    map->set_commits_left(2)
        .set_undos_left(1)
        .add_object(location(0, 2), object('>'))
        .add_object(location(1, 2), object('4', ManipulationLevel::Push))
        .add_object(location(2, 1), object('+', ManipulationLevel::Push))
        .add_object(location(3, 1), object('=', ManipulationLevel::Push));

    // When
    std::unique_ptr<core::Game> game = map->create_game();
    const core::MapView view = game->view();

    // Then
    EXPECT_EQ(view.commits_left, 2);
    EXPECT_EQ(view.undos_left, 1);
    ASSERT_TRUE(view.player.has_value());
    EXPECT_EQ(view.player->location, (core::Location{.x = 0, .y = 2}));
    EXPECT_EQ(view.player->symbol, '>');
    EXPECT_EQ(symbol_at(view, 0, 2), ' ');
    EXPECT_EQ(symbol_at(view, 1, 2), '4');
    EXPECT_EQ(symbol_at(view, 2, 1), '+');
    EXPECT_EQ(symbol_at(view, 3, 1), '=');
}

TEST(CoreGameTest, GivenMapBuilderWhenSavingAndLoadingThenMapIsPreserved)
{
    // Given
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "type_erasure_roundtrip_map.json";
    std::unique_ptr<core::MapBuilder> original = core::MapBuilder::create();
    original->set_commits_left(3)
        .set_undos_left(7)
        .add_object(location(0, 2), object('^'))
        .add_object(location(1, 2), object('2', ManipulationLevel::Push))
        .add_object(location(1, 1), object('*'));

    // When
    original->save_to_file(path);
    std::unique_ptr<core::MapBuilder> restored = core::MapBuilder::load_from_file(path);

    // Then
    ASSERT_TRUE(restored->view().player.has_value());
    EXPECT_EQ(restored->view().player->location, (core::Location{.x = 0, .y = 2}));
    EXPECT_EQ(restored->view().player->symbol, '^');
    EXPECT_EQ(symbol_at(restored->view(), 0, 2), ' ');
    EXPECT_EQ(symbol_at(restored->view(), 1, 2), '2');
    EXPECT_EQ(symbol_at(restored->view(), 1, 1), '*');
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
    std::unique_ptr<core::MapBuilder> map = core::MapBuilder::create();
    map->add_object(location(0, 0), object('>'))
        .add_object(location(1, 0), object('='))
        .add_object(location(2, 0), object('0', ManipulationLevel::Push));
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
    EXPECT_EQ(symbol_at(game->view(), 0, 0), ' ');
    EXPECT_EQ(symbol_at(game->view(), 1, 0), '=');
    EXPECT_EQ(symbol_at(game->view(), 2, 0), '0');
}

TEST(LoginViewTest, GivenUserCreatesAndSavesMapThenMapIsStoredUnderUserDirectory)
{
    // Given
    const std::filesystem::path root = make_login_test_root();
    CurrentPathGuard current_path(root);
    std::unique_ptr<core::LoginView> login = core::LoginView::create();
    core::User &user = login->create_user("eve");
    std::unique_ptr<core::MapBuilder> map = user.create_new_map();
    map->add_object(location(0, 0), object('>'))
        .add_object(location(1, 0), object('='))
        .add_object(location(2, 0), object('0', ManipulationLevel::Push));

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
    EXPECT_EQ(symbol_at(game->view(), 0, 0), ' ');
    EXPECT_EQ(symbol_at(game->view(), 1, 0), '=');
    EXPECT_EQ(symbol_at(game->view(), 2, 0), '0');
}

TEST(LoginViewTest, GivenMapsFromOtherUsersWhenLoggedInThenAllMapsAreAvailable)
{
    // Given
    const std::filesystem::path root = make_login_test_root();
    CurrentPathGuard current_path(root);
    std::filesystem::create_directories(root / "maps" / "eve");
    std::filesystem::create_directories(root / "maps" / "frank");

    std::unique_ptr<core::MapBuilder> global_map = core::MapBuilder::create();
    global_map->add_object(location(0, 0), object('>'));
    global_map->save_to_file(root / "maps" / "zero.json");

    std::unique_ptr<core::MapBuilder> own_map = core::MapBuilder::create();
    own_map->add_object(location(0, 0), object('>'));
    own_map->save_to_file(root / "maps" / "eve" / "custom.json");

    std::unique_ptr<core::MapBuilder> other_map = core::MapBuilder::create();
    other_map->add_object(location(0, 0), object('>'));
    other_map->save_to_file(root / "maps" / "frank" / "shared.json");

    std::unique_ptr<core::LoginView> login = core::LoginView::create();
    core::User &user = login->create_user("eve");

    // When
    const auto &maps = user.available_maps();

    // Then
    ASSERT_EQ(maps.size(), 3U);
    const auto has_map = [&](const std::string &map_id, const std::string &display_name) {
        return std::any_of(maps.begin(), maps.end(), [&](const core::MapEntry &entry) {
            return entry.map_id == map_id && entry.display_name == display_name;
        });
    };
    EXPECT_TRUE(has_map("eve/custom", "custom"));
    EXPECT_TRUE(has_map("frank/shared", "frank/shared"));
    EXPECT_TRUE(has_map("zero", "zero"));
    EXPECT_TRUE(static_cast<bool>(user.select_map("frank/shared")));
}

TEST(MapBuilderTest, GivenMapWithoutPlayerWhenLoadingIntoBuilderThenParsingSucceeds)
{
    // Given
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "type_erasure_no_player_map.json";
    std::unique_ptr<core::MapBuilder> map = core::MapBuilder::create();
    map->set_commits_left(0)
        .set_undos_left(0)
        .clear_cell(location(0, 0))
        .add_object(location(1, 0), object('1'))
        .add_object(location(2, 0), object('+'))
        .add_object(location(3, 0), object('1'));
    map->save_to_file(path);

    // When
    std::unique_ptr<core::MapBuilder> builder = core::MapBuilder::load_from_file(path);

    // Then
    EXPECT_FALSE(builder->view().player.has_value());
    EXPECT_EQ(symbol_at(builder->view(), 1, 0), '1');
    EXPECT_EQ(symbol_at(builder->view(), 2, 0), '+');
}
