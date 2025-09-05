#pragma once

#include "sfg_utils/ros_utils.tpp"

namespace sfg_utils
{
    template <typename ParameterType, typename... Args>
    auto declare_parameter_if_not_declared(rclcpp::Node &node, const std::string &name, const ParameterType &default_value, Args &&...args)
    {
        if (!node.has_parameter(name))
        {
            node.declare_parameter(name, default_value, std::forward<Args>(args)...);
        }
        return node.get_parameter(name).template get_value<ParameterType>();
    }

    template <typename ParameterType, typename... Args>
    auto declare_parameter_if_not_declared(rclcpp::Node &node, const std::string &name, Args &&...args)
    {
        if (!node.has_parameter(name))
        {
            node.template declare_parameter<ParameterType>(name, std::forward<Args>(args)...);
        }
        return node.get_parameter(name).template get_value<ParameterType>();
    }
}