#pragma once

#include <rclcpp/rclcpp.hpp>

namespace sfg_utils
{
    std::vector<rcl_interfaces::msg::Parameter> extract_parameters(const rclcpp::NodeOptions &options);
}