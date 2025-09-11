#include "sfg_image_transport/republisher.hpp"

#include "image_transport/image_transport.hpp"
#include "image_transport/publisher_plugin.hpp"

namespace sfg_image_transport
{
    std::shared_ptr<pluginlib::ClassLoader<image_transport::PublisherPlugin>> Republisher::s_plugin_loader = nullptr;
    std::mutex Republisher::s_mutex;

    Republisher::Republisher(const rclcpp::NodeOptions &options) : Node("republisher", options)
    {
        std::lock_guard lock(s_mutex);

        if (!s_plugin_loader)
        {
            s_plugin_loader = std::make_shared<pluginlib::ClassLoader<image_transport::PublisherPlugin>>("image_transport", "image_transport::PublisherPlugin");
        }

        // Declare and retrieve ROS parameters.
        m_in_transport = declare_parameter<std::string>(
            "in_transport",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The transport used for the input topic."));

        m_out_transport = declare_parameter<std::string>(
            "out_transport",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The transport used for the output topic."));

        // Set up interfaces.
        auto in_topic = rclcpp::expand_topic_or_service_name("in", get_name(), get_namespace());
        auto out_topic = rclcpp::expand_topic_or_service_name("out", get_name(), get_namespace());

        // Create publisher.
        std::string lookup_name = Plugin::getLookupName(m_out_transport);
        m_publisher_plugin = s_plugin_loader->createUniqueInstance(lookup_name);
        m_publisher_plugin->advertise(this, out_topic, rmw_qos_profile_sensor_data);
        PublishMemberFunction function = &Plugin::publishPtr;

        // Create subscriber.
        m_subscriber = image_transport::create_subscription(
            this,
            in_topic,
            std::bind(function, m_publisher_plugin.get(), std::placeholders::_1),
            m_in_transport,
            rmw_qos_profile_sensor_data);

        in_topic = m_subscriber.getTopic();
        out_topic = m_publisher_plugin->getTopic();

        RCLCPP_INFO(
            this->get_logger(),
            "Started republisher node. Converting from transport '%s' on topic '%s' to transport '%s' on topic '%s'.",
            m_in_transport.c_str(),
            in_topic.c_str(),
            m_out_transport.c_str(),
            out_topic.c_str());
    }
}