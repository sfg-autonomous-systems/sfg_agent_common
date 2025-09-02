#include "sfg_topic_tools/relay.hpp"

namespace sfg_topic_tools
{
    Relay::Relay(const rclcpp::NodeOptions &options) : Node("relay", options)
    {
        // Declare and retrieve ROS parameters.
        m_input_topic = declare_parameter<std::string>(
            "input_topic",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The input topic to subscribe to."));

        m_output_topic = declare_parameter<std::string>(
            "output_topic",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The output topic to publish to."));

        m_msg_type = declare_parameter<std::string>(
            "msg_type",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The msg type published on the input and output topics."));

        auto default_qos = rclcpp::QoS(10);
        auto qos_override_options = rclcpp::QosOverridingOptions({
            rclcpp::QosPolicyKind::Depth,
            rclcpp::QosPolicyKind::Durability,
            rclcpp::QosPolicyKind::History,
            rclcpp::QosPolicyKind::Reliability,
        });

        // We need to manually declare and retrieve the QoS override parameters because
        // rclcpp::GenericPublisher does not do this by itself yet.
        const rclcpp::QoS publisher_qos = rclcpp::detail::declare_qos_parameters(
            qos_override_options,
            *this,
            get_node_topics_interface()->resolve_topic_name(m_output_topic),
            default_qos,
            rclcpp::detail::PublisherQosParametersTraits{});

        m_publisher = create_generic_publisher(
            m_output_topic,
            m_msg_type,
            publisher_qos);

        // We need to manually declare and retrieve the QoS override parameters because
        // rclcpp::GenericSubscription does not do this by itself yet.
        const rclcpp::QoS subscription_qos = rclcpp::detail::declare_qos_parameters(
            qos_override_options,
            *this,
            get_node_topics_interface()->resolve_topic_name(m_input_topic),
            default_qos,
            rclcpp::detail::SubscriptionQosParametersTraits{});

        m_subscriber = create_generic_subscription(
            m_input_topic, m_msg_type, subscription_qos,
            [this](std::shared_ptr<rclcpp::SerializedMessage> msg)
            { m_publisher->publish(*msg); });

        RCLCPP_INFO(get_logger(), "Started relaying messages of type '%s' from topic '%s' to topic '%s'.", m_msg_type.c_str(), m_input_topic.c_str(), m_output_topic.c_str());
    }
}