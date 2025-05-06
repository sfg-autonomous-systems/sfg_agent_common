#pragma once

#include <rclcpp/rclcpp.hpp>
#include <image_transport/image_transport.hpp>
#include "pluginlib/class_loader.hpp"

namespace sfg_image_transport
{
    class Republisher : public rclcpp::Node
    {
    public:
        Republisher(const rclcpp::NodeOptions &options);

    private:
        // ROS parameters
        std::string m_in_transport;

        std::string m_in_topic;
        std::string m_out_topic;

        image_transport::Subscriber m_subscriber;
        image_transport::Publisher m_publisher;
    };
}