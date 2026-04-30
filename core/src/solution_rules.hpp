#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace core::solution_rules
{
const std::vector<char> OPERATORS{'+', '-', '*', '/', '=', ':'};
const std::vector<char> EQUATION_DELIMITERS{'#'};

bool solved_equation(std::string_view grid);

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

EquationResult evaluate_equation(std::string_view grid);
} // namespace core::solution_rules
