#pragma once

#include <chrono>
#include <memory>
#include <rclcpp/rclcpp.hpp>
#include <sfg_agent_msgs/msg/wifi_link_status.hpp>
#include <string>

namespace sfg_hardware_interface
{
    class WifiLinkStatusPublisher : public rclcpp::Node
    {
    public:
        WifiLinkStatusPublisher(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());

    private:
        void publish_link_status();

        // ROS parameters
        std::string m_network_interface;
        std::chrono::milliseconds m_receive_timeout_ms;
        float m_publish_rate;
        std::string m_frame_id;

        rclcpp::Publisher<sfg_agent_msgs::msg::WifiLinkStatus>::SharedPtr m_link_status_publisher;
        rclcpp::TimerBase::SharedPtr m_timer;
    };
}
