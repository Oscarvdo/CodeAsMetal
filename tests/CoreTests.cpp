/** GoogleTest registration reuses the offline behavioral suite. */
#include "CoreCases.h"
#include <gtest/gtest.h>
class CoreBehavior:public testing::TestWithParam<std::size_t>{};
TEST_P(CoreBehavior,PreservesEngineeringInvariant){const auto& c=tests::cases().at(GetParam());SCOPED_TRACE(c.first);EXPECT_NO_THROW(c.second());}
INSTANTIATE_TEST_SUITE_P(CodeAsMetal,CoreBehavior,testing::Range(std::size_t{0},tests::cases().size()));
