#pragma once

#include "game.hpp"
#include <memory>
#include <string>
#include <vector>

namespace core
{

struct MapEntry
{
    std::string map_id;
    std::string display_name;
};

class User
{
  public:
    virtual ~User() = default;

    virtual const std::string &username() const = 0;
    virtual const std::vector<MapEntry> &solved_maps() const = 0;
    virtual const std::vector<MapEntry> &available_maps() const = 0;
    virtual std::unique_ptr<Game> select_map(const std::string &map_id) = 0;
};

struct HighscoreEntry
{
    std::string username;
    int solved_maps{0};
};

class LoginView
{
  public:
    virtual ~LoginView() = default;

    virtual const std::vector<HighscoreEntry> &highscore_list() const = 0;
    virtual User &login_as_user(const std::string &username) = 0;
    virtual User &create_user(const std::string &username) = 0;

    static std::unique_ptr<LoginView> create();
};
} // namespace core
