#pragma once

#include <image_transport/image_transport.hpp>
#include <image_transport/publisher_plugin.hpp>
#include <pluginlib/class_loader.hpp>
#include <rclcpp/rclcpp.hpp>

namespace sfg_image_transport
{
    class Republisher : public rclcpp::Node
    {
    public:
        Republisher(const rclcpp::NodeOptions &options);

    private:
        // ROS parameters
        std::string m_in_transport;
        std::string m_out_transport;

        std::shared_ptr<pluginlib::ClassLoader<image_transport::PublisherPlugin>> m_plugin_loader;
        image_transport::Subscriber m_subscriber;
        pluginlib::UniquePtr<image_transport::PublisherPlugin> m_publisher_plugin;
    };
}