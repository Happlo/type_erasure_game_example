#include "solution_rules.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <map>
#include <string>

namespace
{
using core::solution_rules::EqualityStatus;
using core::solution_rules::EquationResult;
using core::solution_rules::Location;

struct EvaluateEquationCase
{
    std::string grid;
    std::map<char, int> resolved_variables;
    std::map<Location, EqualityStatus> equal_sign_status;
};

void expect_evaluate_equation_result(const EvaluateEquationCase &test_case)
{
    const EquationResult result = core::solution_rules::evaluate_equation(test_case.grid);

    EXPECT_EQ(result.resolved_variables, test_case.resolved_variables);
    EXPECT_EQ(result.equal_sign_status, test_case.equal_sign_status);
}

} // namespace

class SolvedEquationSuccessTest : public ::testing::TestWithParam<std::string>
{
};

class SolvedEquationFailureTest : public ::testing::TestWithParam<std::string>
{
};

TEST_P(SolvedEquationSuccessTest, ReturnsTrue)
{
    EXPECT_TRUE(core::solution_rules::solved_equation(GetParam()));
}

TEST_P(SolvedEquationFailureTest, ReturnsFalse)
{
    EXPECT_FALSE(core::solution_rules::solved_equation(GetParam()));
}

TEST(EvaluateEquationTest, ReturnsExpectedResults)
{
    const std::array test_cases{
        EvaluateEquationCase{
            .grid = "x+y=5#1+1=3\n"
                    "x:2\n"
                    "y:3\n",
            .resolved_variables = {{'x', 2}, {'y', 3}},
            .equal_sign_status = {{{3, 0}, EqualityStatus::Equal},
                                  {{9, 0}, EqualityStatus::NotEqual}},
        },
        EvaluateEquationCase{
            .grid = "x+y=5#7-3=4\n"
                    "x:2\n"
                    "y:x+1\n",
            .resolved_variables = {},
            .equal_sign_status = {{{3, 0}, EqualityStatus::NotEqual},
                                  {{9, 0}, EqualityStatus::Equal}},
        },
    };

    for (const auto &test_case : test_cases)
        expect_evaluate_equation_result(test_case);
}

INSTANTIATE_TEST_SUITE_P(SolutionRules, SolvedEquationSuccessTest,
                         ::testing::Values("1+2=3\n"
                                           "     \n"
                                           "  v  \n",
                                           "1+2+3=6\n", "1+2+3=5+1\n", "1 +  2   +3 =   6\n",
                                           "1+2+3+4+5+6=21\n", "1+5+4=10\n", "40+60+110=210\n",
                                           "7-3=4\n", "7-3=5-1\n", "7*3=21\n", "7*5=40-5\n",
                                           "20/5=4\n", "20/5=2*2\n", "5-10=10-15\n",
                                           "1-1-1-1-1-1=1+1-2-4\n", "12+3#4=4\n", "x:3#x=2+1\n",
                                           "x+y=5#x:2#y:3\n", "x+y=5#y:x+1#x:2\n",
                                           "x+y=5\n"
                                           "x:2\n"
                                           "y:3\n"

                                           ));

INSTANTIATE_TEST_SUITE_P(SolutionRules, SolvedEquationFailureTest,
                         ::testing::Values("1+2=4\n"
                                           "     \n"
                                           "  v  \n",
                                           "      2      \n"
                                           "  2 + 1 + 3 = 4\n"
                                           "             \n"
                                           "      v      \n",
                                           "1+1=3#12+3\n",
                                           "12+3 \n"
                                           "     \n"
                                           "  >  \n",
                                           "x:\n", ":3\n", "a:b\n", "x:3\n", "x:1+2\n",
                                           "x:3#x=2\n"));
