#pragma once

#include <map>
#include <vector>

#include "location.hpp"
#include "map.hpp"

namespace core
{
struct GameResult
{
    std::map<char, int> resolved_variables;
    std::vector<Location> remaining_equal_signs;
    std::map<Location, Object> removed_objects;

    bool solved() const { return remaining_equal_signs.empty(); }
};
} // namespace core
