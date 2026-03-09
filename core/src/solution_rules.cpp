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

bool solved_equation(const std::string_view grid_text)
{
    size_t line_start = 0;
    while (line_start <= grid_text.size())
    {
        const size_t line_end = grid_text.find('\n', line_start);
        const size_t end = (line_end == std::string_view::npos) ? grid_text.size() : line_end;
        std::string line(grid_text.substr(line_start, end - line_start));
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.find('=') != std::string::npos)
        {
            return equation_is_correct(line);
        }

        if (line_end == std::string_view::npos) break;
        line_start = line_end + 1;
    }

    return false;
}
}  // namespace core::solution_rules
