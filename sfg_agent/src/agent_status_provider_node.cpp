#include <rclcpp/rclcpp.hpp>

#include "sfg_agent/agent_status_provider.hpp"

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<sfg_agent::AgentStatusProvider>());
  rclcpp::shutdown();
  return 0;
}
