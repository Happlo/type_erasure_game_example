#include "solution_rules.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <unordered_map>

namespace core::solution_rules
{
namespace
{
bool parse_number(std::string_view text, size_t &pos, int &value)
{
    if (pos >= text.size() || !std::isdigit(static_cast<unsigned char>(text[pos])))
        return false;

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
    return std::find(OPERATORS.begin(), OPERATORS.end(), ch) != OPERATORS.end();
}

bool is_assignable_symbol(const char ch)
{
    return ch != '#' && !is_operator(ch) && !std::isspace(static_cast<unsigned char>(ch));
}

std::optional<int> parse_term(std::string_view text, size_t &pos,
                              const std::unordered_map<char, int> &variables);
std::optional<int> parse_expression(std::string_view text, size_t &pos,
                                    const std::unordered_map<char, int> &variables);

std::optional<int> parse_term(std::string_view text, size_t &pos,
                              const std::unordered_map<char, int> &variables)
{
    int value = 0;
    if (parse_number(text, pos, value))
    {
        while (pos < text.size() && (text[pos] == '*' || text[pos] == '/'))
        {
            const char op = text[pos];
            ++pos;

            int rhs = 0;
            if (!parse_number(text, pos, rhs))
                return std::nullopt;

            if (op == '*')
            {
                value *= rhs;
                continue;
            }

            if (rhs == 0 || (value % rhs) != 0)
                return std::nullopt;
            value /= rhs;
        }

        return value;
    }

    if (pos < text.size() && is_assignable_symbol(text[pos]))
    {
        const char symbol = text[pos];
        ++pos;

        auto it = variables.find(symbol);
        if (it == variables.end())
            return std::nullopt;
        return it->second;
    }

    return std::nullopt;
}

std::optional<int> parse_expression(std::string_view text, size_t &pos,
                                    const std::unordered_map<char, int> &variables)
{
    std::optional<int> value = parse_term(text, pos, variables);
    if (!value.has_value())
        return std::nullopt;

    while (pos < text.size() && (text[pos] == '+' || text[pos] == '-'))
    {
        const char op = text[pos];
        ++pos;

        const std::optional<int> rhs = parse_term(text, pos, variables);
        if (!rhs.has_value())
            return std::nullopt;

        if (op == '+')
            *value += *rhs;
        else
            *value -= *rhs;
    }

    return value;
}

std::optional<int> evaluate_expression(std::string_view text,
                                       const std::unordered_map<char, int> &variables = {})
{
    if (text.empty())
        return std::nullopt;

    size_t pos = 0;
    const std::optional<int> value = parse_expression(text, pos, variables);
    if (!value.has_value() || pos != text.size())
        return std::nullopt;

    return value;
}

std::string strip_space(const std::string &text)
{
    std::string compact;
    compact.reserve(text.size());
    std::copy_if(text.begin(), text.end(), std::back_inserter(compact),
                 [](char ch) { return !std::isspace(static_cast<unsigned char>(ch)); });
    return compact;
}

bool assignment_is_correct(const std::string &assignment,
                           const std::unordered_map<char, int> &variables = {})
{
    const std::string compact = strip_space(assignment);

    if (compact.empty())
        return false;

    const size_t colon_pos = compact.find(':');
    if (colon_pos == std::string::npos)
        return false;
    if (compact.find(':', colon_pos + 1) != std::string::npos)
        return false;

    const std::string_view lhs(compact.data(), colon_pos);
    const std::string_view rhs(compact.data() + colon_pos + 1, compact.size() - colon_pos - 1);

    if (lhs.empty() || rhs.empty())
        return false;
    if (lhs.size() != 1)
        return false;

    const char symbol = lhs[0];
    if (!is_assignable_symbol(symbol))
        return false;

    const std::optional<int> right_value = evaluate_expression(rhs, variables);
    return right_value.has_value();
}

bool equation_is_correct(const std::string &equation,
                         const std::unordered_map<char, int> &variables = {})
{
    const std::string compact = strip_space(equation);

    if (compact.empty())
        return false;

    if (!std::all_of(compact.begin(), compact.end(), [&](char ch) {
            return std::isdigit(static_cast<unsigned char>(ch)) || ch == '=' || is_operator(ch) ||
                   is_assignable_symbol(ch);
        }))
        return false;

    const size_t equals_pos = compact.find('=');
    if (equals_pos == std::string::npos)
        return false;
    if (compact.find('=', equals_pos + 1) != std::string::npos)
        return false;

    const std::string_view lhs(compact.data(), equals_pos);
    const std::string_view rhs(compact.data() + equals_pos + 1, compact.size() - equals_pos - 1);
    if (lhs.empty() || rhs.empty())
        return false;

    const std::optional<int> left_value = evaluate_expression(lhs, variables);
    const std::optional<int> right_value = evaluate_expression(rhs, variables);
    if (!left_value.has_value() || !right_value.has_value())
        return false;

    return *left_value == *right_value;
}

std::vector<std::string> split_segments(std::string_view line)
{
    const std::string compact = strip_space(std::string(line));
    std::vector<std::string> segments;
    size_t pos = 0;
    while (pos <= compact.size())
    {
        size_t end = compact.find('#', pos);
        segments.push_back(compact.substr(pos, end - pos));
        if (end == std::string::npos)
            break;
        pos = end + 1;
    }
    return segments;
}

bool try_process_assignment(const std::string &segment, std::unordered_map<char, int> &variables)
{
    if (!assignment_is_correct(segment, variables))
        return false;
    const size_t colon_pos = segment.find(':');
    const char symbol = segment[colon_pos - 1];
    const std::string_view rhs(segment.data() + colon_pos + 1, segment.size() - colon_pos - 1);
    const std::optional<int> value = evaluate_expression(rhs, variables);
    if (!value.has_value())
        return false;
    if (variables.count(symbol) && variables[symbol] != *value)
        return false;
    variables[symbol] = *value;
    return true;
}

std::unordered_map<char, int> parse_assignments(std::string_view line)
{
    const auto segments = split_segments(line);
    std::unordered_map<char, int> variables;
    // Process assignments in reverse order to allow forward dependencies
    for (auto it = segments.rbegin(); it != segments.rend(); ++it)
    {
        const auto &segment = *it;
        if (segment.find(':') != std::string::npos)
            if (!try_process_assignment(segment, variables))
                return {};
    }
    return variables;
}

bool solve_with_assignments(std::string_view line, const std::unordered_map<char, int> &variables)
{
    const auto segments = split_segments(line);

    for (const auto &segment : segments)
    {
        if (segment.find('=') != std::string::npos)
        {
            if (equation_is_correct(segment, variables))
                return true;
        }
    }

    return false;
}

std::pair<std::string, bool> extract_next_line(std::string_view grid_text, size_t &line_start)
{
    const size_t line_end = grid_text.find('\n', line_start);
    const size_t end = (line_end == std::string_view::npos) ? grid_text.size() : line_end;
    std::string line(grid_text.substr(line_start, end - line_start));
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    bool has_more = line_end != std::string_view::npos;
    line_start = has_more ? line_end + 1 : grid_text.size() + 1;
    return {line, has_more};
}

bool line_is_correct(std::string_view line)
{
    const std::string compact = strip_space(std::string(line));
    if (compact.empty())
        return false;

    auto variables = parse_assignments(line);
    if (variables.empty() && compact.find(':') != std::string::npos)
        return false; // Had assignments but parsing failed

    return solve_with_assignments(line, variables);
}
} // namespace

bool solved_equation(const std::string_view grid_text)
{
    std::vector<std::string> all_assignment_segments;
    std::unordered_map<char, int> global_variables;

    // First pass: collect all assignment segments from all lines
    size_t line_start = 0;
    while (true)
    {
        auto [line, has_more] = extract_next_line(grid_text, line_start);
        const auto segments = split_segments(line);
        for (const auto &segment : segments)
            if (segment.find(':') != std::string::npos)
                all_assignment_segments.push_back(segment);
        if (!has_more)
            break;
    }

    // Process assignments in reverse order globally
    for (auto it = all_assignment_segments.rbegin(); it != all_assignment_segments.rend(); ++it)
    {
        const auto &segment = *it;
        if (!try_process_assignment(segment, global_variables))
            return false; // Invalid assignment
    }

    // Second pass: check equations with global variables
    line_start = 0;
    while (true)
    {
        auto [line, has_more] = extract_next_line(grid_text, line_start);
        const auto segments = split_segments(line);
        for (const auto &segment : segments)
            if (segment.find('=') != std::string::npos)
                if (equation_is_correct(segment, global_variables))
                    return true;
        if (!has_more)
            break;
    }

    return false;
}
} // namespace core::solution_rules
