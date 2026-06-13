#pragma once

#include <map>

#include "location.hpp"

namespace core
{
enum class EqualityStatus
{
    Equal,
    NotEqual,
};

struct GameResult
{
    std::map<char, int> resolved_variables;
    std::map<Location, EqualityStatus> equal_sign_status;
};
} // namespace core
