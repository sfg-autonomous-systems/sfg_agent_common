#pragma once

#include <cstdint>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/timer.hpp>

#include "sfg_agent_msgs/msg/heartbeat.hpp"

namespace sfg_agent
{
    class Heartbeat : public rclcpp::Node
    {
    public:
        Heartbeat();

    private:
        void publish_heartbeat();
        std::string get_sanitized_hostname();

        // ROS parameters
        std::string m_override_hostname;
        int8_t m_interval;

        std::string m_hostname;
        rclcpp::Publisher<sfg_agent_msgs::msg::Heartbeat>::SharedPtr m_publisher;
        rclcpp::TimerBase::SharedPtr m_timer;
    };
}