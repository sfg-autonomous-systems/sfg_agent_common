#pragma once

#include <rclcpp/rclcpp.hpp>

std::vector<rcl_interfaces::msg::Parameter> extract_parameters(const rclcpp::NodeOptions &options);