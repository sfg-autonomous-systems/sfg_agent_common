#include "sfg_hardware_interface/wifi_link_status_publisher.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

#include "sfg_utils/agent_utils.hpp"
#include "sfg_utils/fqn/ros_fqn_builder.hpp"
#include "sfg_utils/ros_utils.hpp"

namespace sfg_hardware_interface
{
    WifiLinkStatusPublisher::WifiLinkStatusPublisher(const rclcpp::NodeOptions &options) : Node("wifi", options)
    {
        // Declare and retrieve ROS parameters.
        m_network_interface = sfg_utils::ros_utils::declare_parameter_if_not_declared<std::string>(
            *this,
            "network_interface",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__read_only(true)
                .set__description("Name of the Wi-Fi network interface to monitor."));
        // const auto interface_index = if_nametoindex(m_network_interface.c_str());
        // if (interface_index == 0)
        // {
        //     throw std::invalid_argument("Network interface '" + m_network_interface + "' does not exist.");
        // }

        constexpr auto receive_timeout_param_name = "receive_timeout";
        auto receive_timeout_ms = std::chrono::milliseconds(sfg_utils::ros_utils::declare_parameter_if_not_declared(
            *this,
            receive_timeout_param_name,
            200,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__read_only(true)
                .set__integer_range({rcl_interfaces::msg::IntegerRange()
                                         .set__from_value(1)
                                         .set__to_value(std::numeric_limits<std::int32_t>::max())})
                .set__description("Timeout in [ms] for receiving a reply from the kernel.")));
        if (!std::isfinite(receive_timeout_ms.count()))
        {
            throw std::invalid_argument("Parameter '" + std::string(receive_timeout_param_name) + "' must be finite.");
        }
        m_receive_timeout_ms = receive_timeout_ms;

        constexpr auto publish_rate_param_name = "publish_rate";
        m_publish_rate = sfg_utils::ros_utils::declare_parameter_if_not_declared(
            *this,
            publish_rate_param_name,
            5.0,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__read_only(true)
                .set__floating_point_range({rcl_interfaces::msg::FloatingPointRange()
                                                .set__from_value(0.01)
                                                .set__to_value(10)})
                .set__description("Rate in [Hz] at which the interface is read and the link status is published."));
        if (!std::isfinite(m_publish_rate))
        {
            throw std::invalid_argument("Parameter '" + std::string(publish_rate_param_name) + "' must be finite.");
        }

        m_frame_id = sfg_utils::ros_utils::declare_parameter_if_not_declared<std::string>(
            *this,
            "frame_id",
            sfg_utils::agent_utils::sanitize_agent_name(sfg_utils::agent_utils::get_agent_name()) + "/base_link",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__read_only(true)
                .set__description("Frame ID written to the header of the published messages."));

        using namespace sfg_utils::fqn;

        // Set up interfaces.
        m_link_status_publisher = create_publisher<sfg_agent_msgs::msg::WifiLinkStatus>(
            RosFqnBuilder().resource(Resource::LinkStatus).build(RosFqnSegment::Resource),
            10);

        m_timer = create_wall_timer(
            std::chrono::duration<float>(1.0f / m_publish_rate),
            std::bind(&WifiLinkStatusPublisher::publish_link_status, this));

        RCLCPP_INFO(get_logger(), "Started Wi-Fi link status publisher. Publishing link status of Wi-Fi interface '%s' at %.2f Hz on '%s'.", m_network_interface.c_str(), m_publish_rate, m_link_status_publisher->get_topic_name());
    }

    void WifiLinkStatusPublisher::publish_link_status()
    {
    }
}
