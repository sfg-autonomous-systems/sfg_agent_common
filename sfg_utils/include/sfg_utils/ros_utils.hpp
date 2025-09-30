#pragma once

#include <rclcpp/rclcpp.hpp>

namespace sfg_utils::ros_utils
{
    template <typename ReturnType>
    std::vector<ReturnType> extract_parameters(rclcpp::Node &node, const std::string &prefix = "");
    extern template std::vector<rclcpp::Parameter> extract_parameters<rclcpp::Parameter>(rclcpp::Node &node, const std::string &prefix);
    extern template std::vector<rcl_interfaces::msg::Parameter> extract_parameters<rcl_interfaces::msg::Parameter>(rclcpp::Node &node, const std::string &prefix);
    std::unordered_map<std::string, rclcpp::ParameterValue> parse_parameters(const std::string &str);

    // These functions are nice if you want to declare node parameters while also having the option "automatically_declare_parameters_from_overrides" set to true
    // because unlike the regular declare_parameter function, these functions don't declare the parameter if it already exists (which would cause an exception to be thrown).
    template <typename ParameterType, typename... Args>
    auto declare_parameter_if_not_declared(rclcpp::Node &node, const std::string &name, const ParameterType &default_value, Args &&...args);

    template <typename ParameterType, typename... Args>
    auto declare_parameter_if_not_declared(rclcpp::Node &node, const std::string &name, Args &&...args);
}

#include "sfg_utils/ros_utils.tpp"