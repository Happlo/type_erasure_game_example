#include "solution_rules.hpp"

#include <cctype>
#include <string>

namespace core::solution_rules
{
namespace
{
bool equation_is_correct(const std::string& equation)
{
    std::string compact;
    for (char ch : equation)
    {
        if (ch != ' ') compact.push_back(ch);
    }

    if (compact.size() != 5) return false;
    if (!std::isdigit(static_cast<unsigned char>(compact[0]))) return false;
    if (compact[1] != '+') return false;
    if (!std::isdigit(static_cast<unsigned char>(compact[2]))) return false;
    if (compact[3] != '=') return false;
    if (!std::isdigit(static_cast<unsigned char>(compact[4]))) return false;

    const int lhs = compact[0] - '0';
    const int rhs = compact[2] - '0';
    const int result = compact[4] - '0';
    return lhs + rhs == result;
}
}  // namespace

bool solved_equation(const internal::Map& map)
{
    for (int y = 0; y < internal::grid_height(map); ++y)
    {
        std::string row;
        row.reserve(static_cast<size_t>(internal::grid_width(map)));
        for (int x = 0; x < internal::grid_width(map); ++x)
        {
            row.push_back(map.grid[y][x].view().symbol);
        }

        if (row.find('=') != std::string::npos) return equation_is_correct(row);
    }

    return false;
}
}  // namespace core::solution_rules
