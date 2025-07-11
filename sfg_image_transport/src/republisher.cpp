#include "sfg_image_transport/republisher.hpp"

#include "image_transport/image_transport.hpp"
#include "image_transport/publisher_plugin.hpp"

namespace sfg_image_transport
{
    Republisher::Republisher(const rclcpp::NodeOptions &options) : Node("republisher", options),
                                                                   m_plugin_loader("image_transport", "image_transport::PublisherPlugin")
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
                .set__description("The transport used for the output topic."));
        get_parameter(parameter, m_out_transport);

        // Set up interfaces.
        m_in_topic = rclcpp::expand_topic_or_service_name("in", get_name(), get_namespace());
        m_out_topic = rclcpp::expand_topic_or_service_name("out", get_name(), get_namespace());

        // Create publisher.
        std::string lookup_name = Plugin::getLookupName(m_out_transport);
        m_publisher_plugin = m_plugin_loader.createUniqueInstance(lookup_name);
        m_publisher_plugin->advertise(this, m_out_topic);
        PublishMemberFunction function = &Plugin::publishPtr;

        // Create subscriber.
        m_subscriber = image_transport::create_subscription(
            this,
            m_in_topic,
            std::bind(function, m_publisher_plugin.get(), std::placeholders::_1),
            m_in_transport);

        RCLCPP_INFO(
            this->get_logger(),
            "Started republisher node. Converting from %s transport on topic '%s' to %s transport on topic '%s'.",
            m_in_transport.c_str(),
            m_in_topic.c_str(),
            m_out_transport.c_str(),
            m_out_topic.c_str());
    }
}