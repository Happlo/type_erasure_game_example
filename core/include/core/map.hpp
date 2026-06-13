#pragma once

#include "location.hpp"
#include <map>
#include <optional>
#include <vector>

namespace core
{

struct Object
{
    enum class ManipulationLevel
    {
        None,
        Push,
        Pick
    };

    char symbol;
    ManipulationLevel manipulation_level{ManipulationLevel::None};
};

struct Player
{
    Location location;
    char symbol{'v'};
    std::vector<Object> inventory{};
};

struct MapView
{
    int commits_left{0};
    int undos_left{0};
    std::optional<Player> player;
    std::map<Location, Object> objects;
};
} // namespace core
