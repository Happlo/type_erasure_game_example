#include "core/login.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace core
{
namespace
{
using nlohmann::json;
namespace fs = std::filesystem;

constexpr std::string_view kUsersDirectory = "users";
constexpr std::string_view kMapsDirectory = "maps";

bool is_solved(const GameResult &result)
{
    return std::any_of(result.equal_sign_status.begin(), result.equal_sign_status.end(),
                       [](const auto &entry) { return entry.second == EqualityStatus::Equal; });
}

fs::path users_directory() { return fs::path(kUsersDirectory); }

fs::path maps_directory() { return fs::path(kMapsDirectory); }

fs::path user_maps_directory(const std::string &username)
{
    return maps_directory() / username;
}

std::string read_text_file(const fs::path &path)
{
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("Failed to open file: " + path.string());
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

json read_json_file(const fs::path &path) { return json::parse(read_text_file(path)); }

void write_json_file(const fs::path &path, const json &value)
{
    std::ofstream output(path);
    if (!output)
        throw std::runtime_error("Failed to write file: " + path.string());
    output << value.dump(2);
    if (!output)
        throw std::runtime_error("Failed to finish writing file: " + path.string());
}

MapEntry make_map_entry(const fs::directory_entry &entry)
{
    const std::string map_id = entry.path().stem().string();
    return MapEntry{map_id, map_id};
}

MapEntry make_user_map_entry(const std::string &username, const fs::directory_entry &entry,
                             const bool current_user)
{
    const std::string map_name = entry.path().stem().string();
    const std::string display_name = current_user ? map_name : username + "/" + map_name;
    return MapEntry{username + "/" + map_name, display_name};
}

bool is_json_file(const fs::directory_entry &entry)
{
    return entry.is_regular_file() && entry.path().extension() == ".json";
}

bool contains_map_id(const std::vector<MapEntry> &maps, const std::string &map_id)
{
    return std::any_of(maps.begin(), maps.end(),
                       [&](const MapEntry &entry) { return entry.map_id == map_id; });
}

void append_json_maps_from_directory(std::vector<MapEntry> &maps, const fs::path &directory)
{
    if (!fs::exists(directory))
        return;

    for (const fs::directory_entry &entry : fs::directory_iterator(directory))
    {
        if (is_json_file(entry))
            maps.push_back(make_map_entry(entry));
    }
}

void append_user_maps_from_directory(std::vector<MapEntry> &maps, const std::string &username,
                                     const bool current_user)
{
    const fs::path directory = user_maps_directory(username);
    if (!fs::exists(directory))
        return;

    for (const fs::directory_entry &entry : fs::directory_iterator(directory))
    {
        if (is_json_file(entry))
            maps.push_back(make_user_map_entry(username, entry, current_user));
    }
}

void append_all_user_maps_from_directory(std::vector<MapEntry> &maps,
                                         const std::string &current_username)
{
    const fs::path directory = maps_directory();
    if (!fs::exists(directory))
        return;

    for (const fs::directory_entry &entry : fs::directory_iterator(directory))
    {
        if (!entry.is_directory())
            continue;

        const std::string username = entry.path().filename().string();
        append_user_maps_from_directory(maps, username, username == current_username);
    }
}

std::vector<MapEntry> load_available_maps(const std::string &username)
{
    std::vector<MapEntry> maps;
    append_json_maps_from_directory(maps, maps_directory());
    append_all_user_maps_from_directory(maps, username);

    std::sort(maps.begin(), maps.end(), [](const MapEntry &lhs, const MapEntry &rhs) {
        if (lhs.display_name != rhs.display_name)
            return lhs.display_name < rhs.display_name;
        return lhs.map_id < rhs.map_id;
    });
    return maps;
}

MapEntry make_solved_map_entry(std::string map_id) { return MapEntry{map_id, map_id}; }

std::vector<MapEntry> parse_solved_maps(const json &user_json)
{
    std::vector<MapEntry> solved_maps;
    const json solved = user_json.value("solved_maps", json::array());
    solved_maps.reserve(solved.size());

    for (const auto &entry : solved)
    {
        solved_maps.push_back(make_solved_map_entry(entry.get<std::string>()));
    }

    std::sort(solved_maps.begin(), solved_maps.end(),
              [](const MapEntry &lhs, const MapEntry &rhs) { return lhs.map_id < rhs.map_id; });
    return solved_maps;
}

json make_user_json(const std::string &username)
{
    return json{{"username", username}, {"solved_maps", json::array()}};
}

fs::path user_path(const std::string &username) { return users_directory() / (username + ".json"); }

void ensure_users_directory()
{
    std::error_code error;
    fs::create_directories(users_directory(), error);
    if (error)
        throw std::runtime_error("Failed to create users directory: " + error.message());
}

void persist_solved_map(const std::string &username, const std::string &map_id)
{
    const fs::path path = user_path(username);
    json user_json = read_json_file(path);
    json solved_maps = user_json.value("solved_maps", json::array());

    const bool already_solved =
        std::any_of(solved_maps.begin(), solved_maps.end(),
                    [&](const json &entry) { return entry.get<std::string>() == map_id; });
    if (already_solved)
        return;

    solved_maps.push_back(map_id);
    user_json["solved_maps"] = std::move(solved_maps);
    write_json_file(path, user_json);
}

HighscoreEntry load_highscore_entry(const fs::directory_entry &entry)
{
    const json user_json = read_json_file(entry.path());
    const std::string username = user_json.at("username").get<std::string>();
    const int solved_count = static_cast<int>(user_json.value("solved_maps", json::array()).size());
    return HighscoreEntry{username, solved_count};
}

std::vector<HighscoreEntry> load_highscores()
{
    std::vector<HighscoreEntry> scores;
    ensure_users_directory();

    for (const fs::directory_entry &entry : fs::directory_iterator(users_directory()))
    {
        if (is_json_file(entry))
            scores.push_back(load_highscore_entry(entry));
    }

    std::sort(scores.begin(), scores.end(),
              [](const HighscoreEntry &lhs, const HighscoreEntry &rhs) {
                  if (lhs.solved_maps != rhs.solved_maps)
                      return lhs.solved_maps > rhs.solved_maps;
                  return lhs.username < rhs.username;
              });
    return scores;
}

class DefaultUser final : public User
{
  public:
    explicit DefaultUser(std::string username)
        : username_(std::move(username)),
          solved_maps_(parse_solved_maps(read_json_file(user_path(username_)))),
          available_maps_(load_available_maps(username_))
    {
    }

    const std::string &username() const override { return username_; }

    const std::vector<MapEntry> &solved_maps() const override { return solved_maps_; }

    const std::vector<MapEntry> &available_maps() const override
    {
        available_maps_ = load_available_maps(username_);
        return available_maps_;
    }

    std::unique_ptr<Game> select_map(std::string map_id) override
    {
        available_maps_ = load_available_maps(username_);
        if (!contains_map_id(available_maps_, map_id))
            throw std::runtime_error("Unknown map: " + map_id);
        return std::make_unique<UserGame>(
            username_, map_id, solved_maps_,
            Game::load_from_file(maps_directory() / (map_id + ".json")));
    }

    std::unique_ptr<MapBuilder> create_new_map() const override
    {
        return MapBuilder::create_default(user_maps_directory(username_));
    }

  private:
    class UserGame final : public Game
    {
      public:
        UserGame(std::string username, std::string map_id, std::vector<MapEntry> &solved_maps,
                 std::unique_ptr<Game> inner_game)
            : username_(std::move(username)), map_id_(std::move(map_id)), solved_maps_(solved_maps),
              inner_game_(std::move(inner_game))
        {
        }

        GameResult apply_event(const Event event) override
        {
            const GameResult result = inner_game_->apply_event(event);
            maybe_persist_solve(result);
            return result;
        }

        MapView view() const override { return inner_game_->view(); }

      private:
        void maybe_persist_solve(const GameResult &result)
        {
            if (!is_solved(result))
                return;
            if (contains_map_id(solved_maps_, map_id_))
                return;

            persist_solved_map(username_, map_id_);
            solved_maps_.push_back(make_solved_map_entry(map_id_));
            std::sort(
                solved_maps_.begin(), solved_maps_.end(),
                [](const MapEntry &lhs, const MapEntry &rhs) { return lhs.map_id < rhs.map_id; });
        }

        std::string username_;
        std::string map_id_;
        std::vector<MapEntry> &solved_maps_;
        std::unique_ptr<Game> inner_game_;
    };

    std::string username_;
    std::vector<MapEntry> solved_maps_;
    mutable std::vector<MapEntry> available_maps_;
};

class DefaultLoginView final : public LoginView
{
  public:
    const std::vector<HighscoreEntry> &highscore_list() const override
    {
        highscores_ = load_highscores();
        return highscores_;
    }

    User &login_as_user(const std::string &username) override
    {
        if (!fs::exists(user_path(username)))
            throw std::runtime_error("Unknown user: " + username);
        return load_user(username);
    }

    User &create_user(const std::string &username) override
    {
        ensure_users_directory();
        const fs::path path = user_path(username);
        if (fs::exists(path))
            throw std::runtime_error("User already exists: " + username);

        write_json_file(path, make_user_json(username));
        highscores_ = load_highscores();
        return load_user(username);
    }

  private:
    User &load_user(const std::string &username)
    {
        current_user_ = std::make_unique<DefaultUser>(username);
        highscores_ = load_highscores();
        return *current_user_;
    }

    mutable std::vector<HighscoreEntry> highscores_;
    std::unique_ptr<DefaultUser> current_user_;
};
} // namespace

std::unique_ptr<LoginView> LoginView::create() { return std::make_unique<DefaultLoginView>(); }
} // namespace core
