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
        typedef image_transport::PublisherPlugin Plugin;
        typedef void (Plugin::*PublishMemberFunction)(const sensor_msgs::msg::Image::ConstSharedPtr &) const;

        // ROS parameters
        std::string m_in_transport;
        std::string m_out_transport;

        image_transport::Subscriber m_subscriber;
        pluginlib::ClassLoader<image_transport::PublisherPlugin> m_plugin_loader;
        pluginlib::UniquePtr<image_transport::PublisherPlugin> m_publisher_plugin;
    };
}