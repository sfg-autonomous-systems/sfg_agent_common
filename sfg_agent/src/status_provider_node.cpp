#include <rclcpp/rclcpp.hpp>

#include "sfg_agent/status_provider.hpp"

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<sfg_agent::StatusProvider>());
  rclcpp::shutdown();
  return 0;
}
