#include <rclcpp/rclcpp.hpp>

#include "sfg_agent/heartbeat.hpp"

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<sfg_agent::Heartbeat>());
  rclcpp::shutdown();
  return 0;
}
