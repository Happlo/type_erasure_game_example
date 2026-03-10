#include "solution_rules.hpp"

#include <cctype>
#include <optional>
#include <string>

namespace core::solution_rules
{
namespace
{
bool parse_number(std::string_view text, size_t& pos, int& value)
{
    if (pos >= text.size() || !std::isdigit(static_cast<unsigned char>(text[pos]))) return false;

    value = 0;
    while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])))
    {
        value = (value * 10) + (text[pos] - '0');
        ++pos;
    }

    return true;
}

bool is_operator(const char ch)
{
    return ch == '+' || ch == '-' || ch == '*' || ch == '/';
}

std::optional<int> parse_term(std::string_view text, size_t& pos);
std::optional<int> parse_expression(std::string_view text, size_t& pos);

std::optional<int> parse_term(std::string_view text, size_t& pos)
{
    int value = 0;
    if (!parse_number(text, pos, value)) return std::nullopt;

    while (pos < text.size() && (text[pos] == '*' || text[pos] == '/'))
    {
        const char op = text[pos];
        ++pos;

        int rhs = 0;
        if (!parse_number(text, pos, rhs)) return std::nullopt;

        if (op == '*')
        {
            value *= rhs;
            continue;
        }

        if (rhs == 0 || (value % rhs) != 0) return std::nullopt;
        value /= rhs;
    }

    return value;
}

std::optional<int> parse_expression(std::string_view text, size_t& pos)
{
    std::optional<int> value = parse_term(text, pos);
    if (!value.has_value()) return std::nullopt;

    while (pos < text.size() && (text[pos] == '+' || text[pos] == '-'))
    {
        const char op = text[pos];
        ++pos;

        const std::optional<int> rhs = parse_term(text, pos);
        if (!rhs.has_value()) return std::nullopt;

        if (op == '+') *value += *rhs;
        else *value -= *rhs;
    }

    return value;
}

std::optional<int> evaluate_expression(std::string_view text)
{
    if (text.empty()) return std::nullopt;

    size_t pos = 0;
    const std::optional<int> value = parse_expression(text, pos);
    if (!value.has_value() || pos != text.size()) return std::nullopt;

    return value;
}

std::string strip_space(const std::string& text)
{
    std::string compact;
    compact.reserve(text.size());
    for (char ch : text)
    {
        if (!std::isspace(static_cast<unsigned char>(ch))) compact.push_back(ch);
    }
    return compact;
}

bool equation_is_correct(const std::string& equation)
{
    const std::string compact = strip_space(equation);

    for (char ch : compact)
    {
        if (!std::isdigit(static_cast<unsigned char>(ch)) && ch != '=' && !is_operator(ch)) return false;
    }

    if (compact.empty()) return false;

    const size_t equals_pos = compact.find('=');
    if (equals_pos == std::string::npos) return false;
    if (compact.find('=', equals_pos + 1) != std::string::npos) return false;

    const std::string_view lhs(compact.data(), equals_pos);
    const std::string_view rhs(compact.data() + equals_pos + 1, compact.size() - equals_pos - 1);
    if (lhs.empty() || rhs.empty()) return false;

    const std::optional<int> left_value = evaluate_expression(lhs);
    const std::optional<int> right_value = evaluate_expression(rhs);
    if (!left_value.has_value() || !right_value.has_value()) return false;

    return *left_value == *right_value;
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
