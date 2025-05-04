#include "sfg_image_transport/republisher.hpp"

#include "image_transport/image_transport.hpp"
#include "image_transport/publisher_plugin.hpp"

namespace sfg_image_transport
{
    Republisher::Republisher(const rclcpp::NodeOptions &options) : Node("agent_decoder", options)
    {
        // Declare and retrieve ROS parameters.
        std::string parameter = "in_transport";
        declare_parameter<std::string>(
            parameter,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The transport used for the input topic."));
        get_parameter(parameter, m_in_transport);

        parameter = "out_transport";
        declare_parameter<std::string>(
            parameter,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The transport to use for the output topic."));
        get_parameter(parameter, m_out_transport);

        // Set up interfaces.
        m_in_topic = rclcpp::expand_topic_or_service_name("in", get_name(), get_namespace());
        m_out_topic = rclcpp::expand_topic_or_service_name("out", get_name(), get_namespace());

        m_loader = std::make_shared<pluginlib::ClassLoader<image_transport::PublisherPlugin>>(
            "image_transport",
            "image_transport::PublisherPlugin");
        std::string lookup_name = image_transport::PublisherPlugin::getLookupName(m_out_transport);
        m_publisher = std::move(m_loader->createUniqueInstance(lookup_name));
        m_publisher->advertise(this, m_out_topic);

        m_subscriber = image_transport::create_subscription(
            this, m_in_topic,
            std::bind(&image_transport::PublisherPlugin::publishPtr, m_publisher.get(), std::placeholders::_1), m_in_transport);

        RCLCPP_INFO(get_logger(), "Started republisher node.");
    }
}