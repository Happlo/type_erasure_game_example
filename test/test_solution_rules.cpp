#include "solution_rules.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
std::string grid_to_text(const std::vector<std::string>& rows)
{
    std::string out;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        out += rows[i];
        if (i + 1 < rows.size()) out.push_back('\n');
    }
    return out;
}
}  // namespace

TEST(SolutionRulesTest, GivenOneLineValidEquationWhenCheckingThenReturnsTrue)
{
    const std::string grid = grid_to_text({
        "1+2=3",
        "     ",
        "  v  "
    });

    EXPECT_TRUE(core::solution_rules::solved_equation(grid));
}

TEST(SolutionRulesTest, GivenWrongMathEquationWhenCheckingThenReturnsFalse)
{
    const std::string grid = grid_to_text({
        "1+2=4",
        "     ",
        "  v  "
    });

    EXPECT_FALSE(core::solution_rules::solved_equation(grid));
}

TEST(SolutionRulesTest, GivenLongEquationWithTwoPlusSignsWhenCheckingThenReturnsFalse)
{
    const std::string grid = grid_to_text({
        "      2      ",
        "  2 + 1 + 3 = 4",
        "             ",
        "      v      "
    });

    EXPECT_FALSE(core::solution_rules::solved_equation(grid));
}

TEST(SolutionRulesTest, GivenNoEqualsSignWhenCheckingThenReturnsFalse)
{
    const std::string grid = grid_to_text({
        "12+3 ",
        "     ",
        "  >  "
    });

    EXPECT_FALSE(core::solution_rules::solved_equation(grid));
}
