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

fs::path users_directory() { return fs::path(kUsersDirectory); }

fs::path maps_directory() { return fs::path(kMapsDirectory); }

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

bool is_json_file(const fs::directory_entry &entry)
{
    return entry.is_regular_file() && entry.path().extension() == ".json";
}

bool contains_map_id(const std::vector<MapEntry> &maps, const std::string &map_id)
{
    return std::any_of(maps.begin(), maps.end(),
                       [&](const MapEntry &entry) { return entry.map_id == map_id; });
}

std::vector<MapEntry> load_available_maps()
{
    std::vector<MapEntry> maps;
    if (!fs::exists(maps_directory()))
        return maps;

    for (const fs::directory_entry &entry : fs::directory_iterator(maps_directory()))
    {
        if (is_json_file(entry))
            maps.push_back(make_map_entry(entry));
    }

    std::sort(maps.begin(), maps.end(), [](const MapEntry &lhs, const MapEntry &rhs) {
        return lhs.display_name < rhs.display_name;
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
          available_maps_(load_available_maps())
    {
    }

    const std::string &username() const override { return username_; }

    const std::vector<MapEntry> &solved_maps() const override { return solved_maps_; }

    const std::vector<MapEntry> &available_maps() const override { return available_maps_; }

    std::unique_ptr<Game> select_map(const std::string &map_id) override
    {
        if (!contains_map_id(available_maps_, map_id))
            throw std::runtime_error("Unknown map: " + map_id);
        return std::make_unique<UserGame>(
            username_, map_id, solved_maps_,
            Game::from_json(read_text_file(maps_directory() / (map_id + ".json"))));
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

        bool apply_event(const Event event) override
        {
            const bool applied = inner_game_->apply_event(event);
            maybe_persist_solve();
            return applied;
        }

        bool solved() const override { return inner_game_->solved(); }

        MapView view() const override { return inner_game_->view(); }

        std::string to_json() const override { return inner_game_->to_json(); }

      private:
        void maybe_persist_solve()
        {
            if (!inner_game_->solved())
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
    std::vector<MapEntry> available_maps_;
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
