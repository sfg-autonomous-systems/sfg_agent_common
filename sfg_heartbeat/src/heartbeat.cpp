#include "sfg_heartbeat/heartbeat.hpp"

#include <limits.h>
#include <unistd.h>

namespace sfg_heartbeat
{
    Heartbeat::Heartbeat() : Node("heartbeat")
    {
        RCLCPP_INFO(get_logger(), "Starting heartbeat.");

        declare_parameter("interval", 1, rcl_interfaces::msg::ParameterDescriptor().set__description("Interval in seconds between heartbeats."));
        get_parameter("interval", m_interval);

        m_publisher = create_publisher<sfg_heartbeat_msgs::msg::Heartbeat>(
            "heartbeat", rclcpp::QoS(rclcpp::KeepLast(1)).keep_last(1).reliable());
        m_timer = create_wall_timer(
            std::chrono::duration<float>(m_interval),
            std::bind(&Heartbeat::publish_heartbeat, this));

        char hostname[HOST_NAME_MAX];

        if (gethostname(hostname, sizeof(hostname)) != 0)
        {
            throw std::runtime_error("Failed to get hostname: " + std::string(strerror(errno)));
        }

        m_hostname = std::string(hostname);
    }

    void Heartbeat::publish_heartbeat()
    {
        auto msg = sfg_heartbeat_msgs::msg::Heartbeat();
        msg.header.stamp = now();
        msg.hostname = m_hostname;
        msg.interval = m_interval;
        m_publisher->publish(msg);
    }
}