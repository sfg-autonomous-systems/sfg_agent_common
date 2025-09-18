#include "sfg_agent/status_provider.hpp"

#include <cctype>
#include <limits.h>
#include <regex>
#include <unistd.h>
#include <yaml-cpp/yaml.h>

#include "sfg_agent/constants.hpp"
#include "sfg_utils/agent_utils.hpp"
#include "sfg_utils/fqn/ros_fqn_builder.hpp"

namespace sfg_agent
{
    StatusProvider::StatusProvider(const rclcpp::NodeOptions &options) : Node("status_provider", options)
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

        using namespace sfg_utils::fqn;

        // Set up interfaces.
        m_heartbeat_publisher = create_publisher<sfg_agent_msgs::msg::Heartbeat>(
            RosFqnBuilder().scope(Scope::Global).resource(Resource::AgentHeartbeat).build(),
            rclcpp::SensorDataQoS());
        m_heartbeat_timer = create_wall_timer(
            std::chrono::seconds(agent_heartbeat_interval),
            std::bind(&StatusProvider::publish_heartbeat, this));
        m_get_metadata_service = create_service<sfg_agent_msgs::srv::GetMetadata>(
            RosFqnBuilder().scope(Scope::Global).agent().resource(Resource::Custom, "get_metadata").build(),
            std::bind(&StatusProvider::get_metadata_callback, this, std::placeholders::_1, std::placeholders::_2));

        RCLCPP_INFO(get_logger(), "Started agent status provider for '%s'.", m_agent_name.c_str());
    }

    void StatusProvider::publish_heartbeat()
    {
        auto msg = sfg_agent_msgs::msg::Heartbeat();
        msg.header.stamp = now();
        msg.agent_name = m_agent_name;
        m_heartbeat_publisher->publish(msg);
    }

    void StatusProvider::get_metadata_callback(
        [[maybe_unused]] const std::shared_ptr<sfg_agent_msgs::srv::GetMetadata::Request> request,
        std::shared_ptr<sfg_agent_msgs::srv::GetMetadata::Response> response)
    {
        RCLCPP_INFO(get_logger(), "Received request for agent.");
        *response = m_get_metadata_response;
    }

    bool StatusProvider::load_metadata(const std::filesystem::path &filepath)
    {
        if (filepath.empty())
        {
            RCLCPP_WARN(get_logger(), "No metadata file specified. Did you forget to specify one?");
            return true;
        }

        m_get_metadata_response.metadata.agent_name = m_agent_name;

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
                m_get_metadata_response.metadata.cameras = config["cameras"].as<std::vector<std::string>>();
            }

            if (config["lidars"])
            {
                m_get_metadata_response.metadata.lidars = config["lidars"].as<std::vector<std::string>>();
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