#pragma once

#include <cstdint>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/timer.hpp>

#include "sfg_heartbeat_msgs/msg/heartbeat.hpp"

namespace sfg_heartbeat
{
    class Heartbeat : public rclcpp::Node
    {
    public:
        Heartbeat();

    private:
        void publish_heartbeat();

        rclcpp::Publisher<sfg_heartbeat_msgs::msg::Heartbeat>::SharedPtr m_publisher;
        rclcpp::TimerBase::SharedPtr m_timer;

        int8_t m_interval;
        std::string m_hostname;
    };
}