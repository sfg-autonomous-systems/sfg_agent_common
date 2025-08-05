#include "sfg_agent/agent_status_provider.hpp"

#include <cctype>
#include <limits.h>
#include <regex>
#include <unistd.h>
#include <yaml-cpp/yaml.h>

#include "sfg_agent/agent_heartbeat_constants.hpp"
#include "sfg_utils/agent_utils.hpp"

namespace sfg_agent
{
    AgentStatusProvider::AgentStatusProvider(const rclcpp::NodeOptions &options) : Node("agent_status_provider", options)
    {
        // Declare and retrieve ROS parameters.
        m_metadata_filepath = declare_parameter(
            "metadata_filepath",
            "",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The filepath pointing to the yaml file containing the metadata of the agent."));

        m_agent_name = sfg_utils::get_agent_name();

        if (!load_metadata(m_metadata_filepath))
        {
            RCLCPP_ERROR(get_logger(), "Failed to load metadata from '%s'.", m_metadata_filepath.c_str());
            throw std::runtime_error("Failed to load metadata.");
        }

        // Set up interfaces.
        m_heartbeat_publisher = create_publisher<sfg_agent_msgs::msg::AgentHeartbeat>(
            "/global/agent_heartbeat",
            rclcpp::SensorDataQoS());
        m_heartbeat_timer = create_wall_timer(
            std::chrono::seconds(AGENT_HEARTBEAT_INTERVAL),
            std::bind(&AgentStatusProvider::publish_heartbeat, this));
        m_get_metadata_service = create_service<sfg_agent_msgs::srv::GetMetadata>(
            "/global/" + sfg_utils::sanitize_agent_name(m_agent_name) + "/get_metadata",
            std::bind(&AgentStatusProvider::get_metadata_callback, this, std::placeholders::_1, std::placeholders::_2));

        RCLCPP_INFO(get_logger(), "Started agent status provider for '%s'.", m_agent_name.c_str());
    }

    void AgentStatusProvider::publish_heartbeat()
    {
        auto msg = sfg_agent_msgs::msg::AgentHeartbeat();
        msg.header.stamp = now();
        msg.agent_name = m_agent_name;
        m_heartbeat_publisher->publish(msg);
    }

    void AgentStatusProvider::get_metadata_callback(
        [[maybe_unused]] const std::shared_ptr<sfg_agent_msgs::srv::GetMetadata::Request> request,
        std::shared_ptr<sfg_agent_msgs::srv::GetMetadata::Response> response)
    {
        RCLCPP_INFO(get_logger(), "Received request for agent.");
        *response = m_get_agent_response;
    }

    bool AgentStatusProvider::load_metadata(const std::filesystem::path &filepath)
    {
        if (filepath.empty())
        {
            RCLCPP_WARN(get_logger(), "No metadata file specified. Did you forget to specify one?");
            return true;
        }

        m_get_agent_response.metadata.agent_name = m_agent_name;

        // Load the YAML file.
        try
        {
            YAML::Node config = YAML::LoadFile(filepath.string());

            if (!config.IsMap())
            {
                RCLCPP_ERROR(get_logger(), "Metadata file '%s' is not a valid YAML map.", filepath.c_str());
                return false;
            }

            if (config["cameras"])
            {
                m_get_agent_response.metadata.cameras = config["cameras"].as<std::vector<std::string>>();
            }

            if (config["lidars"])
            {
                m_get_agent_response.metadata.lidars = config["lidars"].as<std::vector<std::string>>();
            }
        }
        catch (const YAML::Exception &exception)
        {
            RCLCPP_ERROR(get_logger(), "Failed to parse metadata file '%s': %s", filepath.c_str(), exception.what());
            return false;
        }

        return true;
    }
}