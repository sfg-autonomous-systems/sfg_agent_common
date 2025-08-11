#include <gtest/gtest.h>

#include "sfg_utils/fqn/ros_fqn_builder.hpp"

using namespace sfg_utils::fqn;

class RosFQNBuilderTest : public ::testing::Test
{
protected:
    RosFQNBuilder builder;
};

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}