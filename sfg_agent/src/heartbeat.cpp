#include "sfg_agent/heartbeat.hpp"

#include <cctype>
#include <limits.h>
#include <regex>
#include <unistd.h>

#include "sfg_utils/sanitize_hostname.hpp"

namespace sfg_agent
{
    Heartbeat::Heartbeat() : Node("heartbeat")
    {
        RCLCPP_INFO(get_logger(), "Starting heartbeat.");

        // Declare and retrieve ROS parameters.
        declare_parameter("override_hostname", "", rcl_interfaces::msg::ParameterDescriptor().set__description("Override the hostname published."));
        get_parameter("override_hostname", m_override_hostname);
        declare_parameter("interval", 1, rcl_interfaces::msg::ParameterDescriptor().set__description("Interval in seconds between heartbeats."));
        get_parameter("interval", m_interval);

        m_hostname = get_sanitized_hostname();

        // Set up interfaces.
        m_publisher = create_publisher<sfg_agent_msgs::msg::Heartbeat>(
            "heartbeat", rclcpp::QoS(rclcpp::KeepLast(1)).keep_last(1).reliable());
        m_timer = create_wall_timer(
            std::chrono::duration<float>(m_interval),
            std::bind(&Heartbeat::publish_heartbeat, this));
    }

    void Heartbeat::publish_heartbeat()
    {
        auto msg = sfg_agent_msgs::msg::Heartbeat();
        msg.header.stamp = now();
        msg.hostname = m_hostname;
        msg.interval = m_interval;
        m_publisher->publish(msg);
    }

    std::string Heartbeat::get_sanitized_hostname()
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