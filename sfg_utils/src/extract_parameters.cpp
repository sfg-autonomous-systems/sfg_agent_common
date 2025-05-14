#include "sfg_utils/extract_parameters.hpp"

std::vector<rcl_interfaces::msg::Parameter> extract_parameters(const rclcpp::NodeOptions &options)
{
    const auto &overrides = options.parameter_overrides();
    std::vector<rcl_interfaces::msg::Parameter> parameters;
    parameters.reserve(overrides.size());

    std::transform(
        overrides.begin(),
        overrides.end(),
        std::back_inserter(parameters),
        [](const rclcpp::Parameter &parameter)
        {
            return parameter.to_parameter_msg();
        });

    return parameters;
}