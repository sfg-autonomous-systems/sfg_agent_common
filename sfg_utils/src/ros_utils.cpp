#include "sfg_utils/ros_utils.hpp"

#include <regex>

namespace sfg_utils::ros_utils
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

    std::unordered_map<std::string, rclcpp::ParameterValue> parse_parameters(const std::string &str)
    {
        // Regex featuring 4 capture groups:
        // 4. Quoted value ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
        // 3. Unquoted value ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓           ┃
        // 2. Unquoted key ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓              ┃           ┃
        // 1. Quoted key ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓           ┃              ┃           ┃
        //                                                ┃           ┃              ┃           ┃
        //                                          ┏━━━━━┻━━━━━━┓ ┏━━┻━━┓      ┏━━━━┻━━━━━┓ ┏━━━┻━━━┓
        static const std::regex key_value_regex("(?:\"([^\":=]+)\"|([^ ]+)):=(?:\"([^\"]+)\"|([^\" ]+))");

        std::unordered_map<std::string, rclcpp::ParameterValue> result;
        std::sregex_iterator begin(str.begin(), str.end(), key_value_regex);
        std::sregex_iterator end;

        auto allocator = rcl_get_default_allocator();

        for (auto iterator = begin; iterator != end; ++iterator)
        {
            if (iterator->size() != 5) // The full match + 4 capture groups
            {
                continue;
            }

            // The key is either in group 1 (quoted) or group 2 (unquoted).
            std::string key = (*iterator)[1].matched ? (*iterator)[1].str() : (*iterator)[2].str();
            // The value is either in group 3 (quoted) or group 4 (unquoted).
            std::string value = (*iterator)[3].matched ? (*iterator)[3].str() : (*iterator)[4].str();

            auto params = rcl_yaml_node_struct_init_with_capacity(1, allocator);

            if (!params)
            {
                continue;
            }

            if (rcl_parse_yaml_value("dummy", key.c_str(), value.c_str(), params) && params->num_nodes > 0 && params->params[0].num_params > 0)
            {
                result.emplace(key, rclcpp::parameter_value_from(&params->params[0].parameter_values[0]));
            }
            rcl_yaml_node_struct_fini(params);
        }
        return result;
    }
}