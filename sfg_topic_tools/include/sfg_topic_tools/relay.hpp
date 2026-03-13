#pragma once

#include <rclcpp/rclcpp.hpp>

namespace sfg_topic_tools
{
    class Relay : public rclcpp::Node
    {
    public:
        Relay(const rclcpp::NodeOptions &options);

    private:
        // ROS parameters
        std::string m_input_topic;
        std::string m_output_topic;
        std::string m_msg_type;
        std::string m_qos_profile;

        rclcpp::GenericPublisher::SharedPtr m_publisher;
        rclcpp::GenericSubscription::SharedPtr m_subscriber;
    };
}