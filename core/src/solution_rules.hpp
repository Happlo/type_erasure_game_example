#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace core::solution_rules
{
const std::vector<char> OPERATORS {'+', '-', '*', '/', '=', ':'};
const std::vector<char> EQUATION_DELIMITERS {'#'};

bool solved_equation(std::string_view grid_text);
}  // namespace core::solution_rules
