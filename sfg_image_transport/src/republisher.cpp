#include "sfg_image_transport/republisher.hpp"

#include "image_transport/image_transport.hpp"
#include "image_transport/publisher_plugin.hpp"

namespace sfg_image_transport
{
    Republisher::Republisher(const rclcpp::NodeOptions &options) : Node("republisher", options)
    {
        // Declare and retrieve ROS parameters.
        std::string parameter = "in_transport";
        declare_parameter<std::string>(
            parameter,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The transport used for the input topic."));
        get_parameter(parameter, m_in_transport);

        // Set up interfaces.
        m_in_topic = rclcpp::expand_topic_or_service_name("in", get_name(), get_namespace());
        m_out_topic = rclcpp::expand_topic_or_service_name("out", get_name(), get_namespace());

        m_publisher = image_transport::create_publisher(
            this, m_out_topic);

        m_subscriber = image_transport::create_subscription(
            this, m_in_topic,
            [this](const sensor_msgs::msg::Image::ConstSharedPtr &msg)
            {
                m_publisher.publish(msg);
            },
            m_in_transport);

        RCLCPP_INFO(get_logger(), "Started republisher node.");
    }
}