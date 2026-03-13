#include "sfg_image_transport/republisher.hpp"

#include "image_transport/image_transport.hpp"
#include "sfg_pluginlib/plugin_loader.hpp"

namespace sfg_image_transport
{
    Republisher::Republisher(const rclcpp::NodeOptions &options)
        : Node("republisher", options),
          m_publisher_plugin_loader(sfg_pluginlib::PluginLoader<image_transport::PublisherPlugin>::get("image_transport", "image_transport::PublisherPlugin")),
          m_subscriber_plugin_loader(sfg_pluginlib::PluginLoader<image_transport::SubscriberPlugin>::get("image_transport", "image_transport::SubscriberPlugin"))
    {
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
        std::string lookup_name = image_transport::PublisherPlugin::getLookupName(m_out_transport);
        m_publisher_plugin = m_publisher_plugin_loader->createSharedInstance(lookup_name);
        m_publisher_plugin->advertise(this, out_topic, rmw_qos_profile_sensor_data);
        auto weak_publisher_plugin = std::weak_ptr<image_transport::PublisherPlugin>(m_publisher_plugin);

        // ToDo: We should only subscribe to the input topic if there is a subscriber on the output topic.
        //       But ROS2 Humble does not support the corresponding subscription options callback. It's a ROS2 Iron feature...
        lookup_name = image_transport::SubscriberPlugin::getLookupName(m_in_transport);
        m_subscriber_plugin = m_subscriber_plugin_loader->createSharedInstance(lookup_name);
        m_subscriber_plugin->subscribe(
            this,
            in_topic,
            [weak_publisher_plugin](const sensor_msgs::msg::Image::ConstSharedPtr &msg)
            {
                if (auto publisher_plugin = weak_publisher_plugin.lock(); publisher_plugin && publisher_plugin->getNumSubscribers() > 0)
                {
                    publisher_plugin->publishPtr(msg);
                }
            },
            rmw_qos_profile_sensor_data);

        in_topic = m_subscriber_plugin->getTopic();
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