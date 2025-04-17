#include <rclcpp/rclcpp.hpp>

#include "sfg_agent/metadata_provider.hpp"

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<sfg_agent::MetadataProvider>());
    rclcpp::shutdown();
    return 0;
}
