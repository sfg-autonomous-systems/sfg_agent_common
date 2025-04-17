#include <rclcpp/rclcpp.hpp>

#include "sfg_agent/agent_decoder.hpp"

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::executors::MultiThreadedExecutor executor;
    auto node = std::make_shared<sfg_agent::AgentDecoder>(executor);
    executor.add_node(node);
    executor.spin();
    rclcpp::shutdown();
    return 0;
}
