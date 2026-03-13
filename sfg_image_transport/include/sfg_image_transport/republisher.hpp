#pragma once

#include <image_transport/publisher_plugin.hpp>
#include <image_transport/subscriber_plugin.hpp>
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

        std::shared_ptr<pluginlib::ClassLoader<image_transport::PublisherPlugin>> m_publisher_plugin_loader;
        std::shared_ptr<pluginlib::ClassLoader<image_transport::SubscriberPlugin>> m_subscriber_plugin_loader;
        std::shared_ptr<image_transport::PublisherPlugin> m_publisher_plugin;
        std::shared_ptr<image_transport::SubscriberPlugin> m_subscriber_plugin;
    };
}