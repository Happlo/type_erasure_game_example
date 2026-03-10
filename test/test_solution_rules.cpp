#include "solution_rules.hpp"

#include <gtest/gtest.h>

#include <string>

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

INSTANTIATE_TEST_SUITE_P(
    SolutionRules,
    SolvedEquationSuccessTest,
    ::testing::Values(
        "1+2=3\n"
        "     \n"
        "  v  \n",
        "1+2+3=6\n",
        "1+2+3=5+1\n",
        "1 +  2   +3 =   6\n",
        "1+2+3+4+5+6=21\n",
        "1+5+4=10\n",
        "40+60+110=210\n",
        "7-3=4\n",
        "7-3=5-1\n",
        "7*3=21\n",
        "7*5=40-5\n",
        "20/5=4\n",
        "20/5=2*2\n",
        "5-10=10-15\n",
        "1-1-1-1-1-1=1+1-2-4\n"
    ));

INSTANTIATE_TEST_SUITE_P(
    SolutionRules,
    SolvedEquationFailureTest,
    ::testing::Values(
        "1+2=4\n"
        "     \n"
        "  v  \n",
        "      2      \n"
        "  2 + 1 + 3 = 4\n"
        "             \n"
        "      v      \n",
        "12+3 \n"
        "     \n"
        "  >  \n"));
