#include "sfg_utils/ros_utils.hpp"

namespace sfg_utils
{
    template <typename ReturnType>
    std::vector<ReturnType> extract_parameters(rclcpp::Node &node, const std::string &prefix)
    {
        std::map<std::string, rclcpp::Parameter> parameters;
        node.get_node_parameters_interface()->get_parameters_by_prefix(prefix, parameters);

        std::vector<ReturnType> extracted_parameters;
        extracted_parameters.reserve(parameters.size());

        for (const auto &[name, parameter] : parameters)
        {
            if constexpr (std::is_same<ReturnType, rclcpp::Parameter>::value)
            {
                extracted_parameters.push_back(rclcpp::Parameter(name, parameter.get_parameter_value()));
            }
            else if constexpr (std::is_same<ReturnType, rcl_interfaces::msg::Parameter>::value)
            {
                extracted_parameters.push_back(rclcpp::Parameter(name, parameter.get_parameter_value()).to_parameter_msg());
            }
        }

        return extracted_parameters;
    }

    template std::vector<rclcpp::Parameter> extract_parameters<rclcpp::Parameter>(rclcpp::Node &node, const std::string &prefix);
    template std::vector<rcl_interfaces::msg::Parameter> extract_parameters<rcl_interfaces::msg::Parameter>(rclcpp::Node &node, const std::string &prefix);
}