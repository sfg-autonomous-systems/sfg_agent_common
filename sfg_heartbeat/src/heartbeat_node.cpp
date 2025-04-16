#include "rclcpp/rclcpp.hpp"

#include "sfg_heartbeat/heartbeat.hpp"

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<sfg_heartbeat::Heartbeat>());
  rclcpp::shutdown();
  return 0;
}
