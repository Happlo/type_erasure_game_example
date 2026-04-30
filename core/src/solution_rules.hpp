#pragma once

#include "core/equation_result.hpp"

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

EquationResult evaluate_equation(std::string_view grid);
} // namespace core::solution_rules
