#include "sfg_agent/status_provider.hpp"

#include <cctype>
#include <limits.h>
#include <regex>
#include <unistd.h>
#include <yaml-cpp/yaml.h>

#include "sfg_utils/sanitize_hostname.hpp"

namespace sfg_agent
{
    StatusProvider::StatusProvider() : Node("status_provider")
    {
        RCLCPP_INFO(get_logger(), "Starting status provider.");

        // Declare and retrieve ROS parameters.
        declare_parameter("override_hostname", "", rcl_interfaces::msg::ParameterDescriptor().set__description("Override the hostname published."));
        get_parameter("override_hostname", m_override_hostname);
        declare_parameter("heartbeat_interval", 1, rcl_interfaces::msg::ParameterDescriptor().set__description("Interval in seconds between heartbeats."));
        get_parameter("heartbeat_interval", m_heartbeat_interval);
        declare_parameter("metadata_filepath", "", rcl_interfaces::msg::ParameterDescriptor().set__description("The filepath pointing to the yaml file containing the metadata."));
        get_parameter("metadata_filepath", m_metadata_filepath);

        if (!load_metadata(m_metadata_filepath))
        {
            RCLCPP_ERROR(get_logger(), "Failed to load metadata from '%s'.", m_metadata_filepath.c_str());
            throw std::runtime_error("Failed to load metadata.");
        }

        m_hostname = get_sanitized_hostname();

        // Set up interfaces.
        m_heartbeat_publisher = create_publisher<sfg_agent_msgs::msg::Heartbeat>(
            "/global/agent_heartbeat", rclcpp::QoS(rclcpp::KeepLast(1)).keep_last(1).reliable());
        m_timer = create_wall_timer(
            std::chrono::duration<float>(m_heartbeat_interval),
            std::bind(&StatusProvider::publish_heartbeat, this));
        m_metadata_service = create_service<sfg_agent_msgs::srv::GetMetadata>(
            "/global/" + m_hostname + "/get_metadata",
            std::bind(&StatusProvider::get_metadata, this, std::placeholders::_1, std::placeholders::_2));
    }

    void StatusProvider::publish_heartbeat()
    {
        auto msg = sfg_agent_msgs::msg::Heartbeat();
        msg.header.stamp = now();
        msg.hostname = m_hostname;
        msg.interval = m_heartbeat_interval;
        m_heartbeat_publisher->publish(msg);
    }

    void StatusProvider::get_metadata([[maybe_unused]] const std::shared_ptr<sfg_agent_msgs::srv::GetMetadata::Request> request,
                                      std::shared_ptr<sfg_agent_msgs::srv::GetMetadata::Response> response)
    {
        RCLCPP_INFO(get_logger(), "Received request for metadata.");
        *response = m_metadata_response;
    }

    bool StatusProvider::load_metadata(const std::filesystem::path &filepath)
    {
        if (filepath.empty())
        {
            RCLCPP_WARN(get_logger(), "Note metadata file specified. Did you forget to specify one?");
            return true;
        }

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
                m_metadata_response.capabilities |= sfg_agent_msgs::srv::GetMetadata::Response::CAPABILITY_CAMERA;
                m_metadata_response.cameras = config["cameras"].as<std::vector<std::string>>();
            }

            if (config["lidars"])
            {
                m_metadata_response.capabilities |= sfg_agent_msgs::srv::GetMetadata::Response::CAPABILITY_LIDAR;
                m_metadata_response.lidars = config["lidars"].as<std::vector<std::string>>();
            }
        }
        catch (const YAML::Exception &exception)
        {
            RCLCPP_ERROR(get_logger(), "Failed to parse metadata file '%s': %s", filepath.c_str(), exception.what());
            return false;
        }

        return true;
    }

    std::string StatusProvider::get_sanitized_hostname()
    {
        if (!m_override_hostname.empty())
        {
            return sfg_utils::sanitize_hostname(m_override_hostname);
        }

        char hostname[HOST_NAME_MAX + 1];

        if (gethostname(hostname, sizeof(hostname)) != 0)
        {
            throw std::runtime_error("Failed to get hostname: " + std::string(strerror(errno)));
        }

        return sfg_utils::sanitize_hostname(hostname);
    }
}