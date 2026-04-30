#pragma once

#include <map>

namespace core
{
struct Location
{
    int x{0};
    int y{0};

    auto operator<=>(const Location &) const = default;
};

enum class EqualityStatus
{
    Equal,
    NotEqual,
};

struct EquationResult
{
    std::map<char, int> resolved_variables;
    std::map<Location, EqualityStatus> equal_sign_status;
};
} // namespace core
