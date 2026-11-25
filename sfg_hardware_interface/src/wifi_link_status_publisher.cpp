#include "sfg_hardware_interface/wifi_link_status_publisher.hpp"

#include <limits>
#include <stdexcept>

#include "sfg_hardware_interface/detail/network_utils.hpp"
#include "sfg_hardware_interface/detail/nl80211_client.hpp"
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

        m_receive_timeout_ms = std::chrono::milliseconds(sfg_utils::ros_utils::declare_parameter_if_not_declared(
            *this,
            "receive_timeout",
            200,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__read_only(true)
                .set__integer_range({rcl_interfaces::msg::IntegerRange()
                                         .set__from_value(1)
                                         .set__to_value(std::numeric_limits<std::int32_t>::max())})
                .set__description("Timeout in [ms] for receiving a reply from the kernel.")));

        m_publish_rate = sfg_utils::ros_utils::declare_parameter_if_not_declared(
            *this,
            "publish_rate",
            5.0f,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__read_only(true)
                .set__floating_point_range({rcl_interfaces::msg::FloatingPointRange()
                                                .set__from_value(0.01)
                                                .set__to_value(10)})
                .set__description("Rate in [Hz] at which the interface is read and the link status is published."));

        m_frame_id = sfg_utils::ros_utils::declare_parameter_if_not_declared<std::string>(
            *this,
            "frame_id",
            sfg_utils::agent_utils::sanitize_agent_name(sfg_utils::agent_utils::get_agent_name()) + "/base_link",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__read_only(true)
                .set__description("Frame ID written to the header of the published messages."));

        m_antenna_frame_ids = sfg_utils::ros_utils::declare_parameter_if_not_declared<std::vector<std::string>>(
            *this,
            "antenna_frame_ids",
            {},
            rcl_interfaces::msg::ParameterDescriptor()
                .set__read_only(true)
                .set__description("Frame IDs of the individual antennas ordered by their chain index."));

        m_nl80211_client = std::make_unique<detail::Nl80211Client>(m_receive_timeout_ms);
        validate_client(*m_nl80211_client);

        using namespace sfg_utils::fqn;

        // Set up interfaces.
        m_link_status_publisher = create_publisher<sfg_hardware_interface_msgs::msg::WifiLinkStatus>(
            RosFqnBuilder().resource(Resource::LinkStatus).build(RosFqnSegment::Resource),
            10);

        m_timer = create_wall_timer(
            std::chrono::duration<float>(1.0f / m_publish_rate),
            std::bind(&WifiLinkStatusPublisher::publish_link_status, this));

        RCLCPP_INFO(get_logger(), "Started Wi-Fi link status publisher. Publishing link status of Wi-Fi interface '%s' at %.2f Hz on '%s'.", m_network_interface.c_str(), m_publish_rate, m_link_status_publisher->get_topic_name());
    }

    WifiLinkStatusPublisher::~WifiLinkStatusPublisher() = default;

    void WifiLinkStatusPublisher::publish_link_status()
    {
        auto msg = std::make_unique<sfg_hardware_interface_msgs::msg::WifiLinkStatus>();
        msg->header.stamp = now();
        msg->header.frame_id = m_frame_id;
        msg->interface_name = m_network_interface;

        try
        {
            if (!m_nl80211_client)
            {
                m_nl80211_client = std::make_unique<detail::Nl80211Client>(m_receive_timeout_ms);
            }

            const auto interface_info = m_nl80211_client->get_interface(m_network_interface);
            const auto station_infos = m_nl80211_client->get_stations(m_network_interface);

            if (!station_infos.empty())
            {
                fill_link_status(*msg, interface_info, station_infos.front());
            }
            else
            {
                m_previous_sample.reset();
            }
        }
        catch (const detail::Nl80211Client::Error &error)
        {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000, "Failed to read Wi-Fi interface '%s': %s", m_network_interface.c_str(), error.what());
            m_nl80211_client.reset();
            m_previous_sample.reset();
        }
        m_link_status_publisher->publish(std::move(msg));
    }

    void WifiLinkStatusPublisher::validate_client(detail::Nl80211Client &client)
    {
        detail::InterfaceInfo interface_info;
        try
        {
            interface_info = client.get_interface(m_network_interface);
        }
        catch (const detail::Nl80211Client::Error &error)
        {
            throw std::invalid_argument("Failed to get Wi-Fi interface '" + m_network_interface + "': " + error.what());
        }

        if (interface_info.m_interface_type != NL80211_IFTYPE_STATION)
        {
            throw std::invalid_argument("Interface '" + m_network_interface + "' is not a Wi-Fi station interface.");
        }
    }

    void WifiLinkStatusPublisher::fill_link_status(sfg_hardware_interface_msgs::msg::WifiLinkStatus &msg, const detail::InterfaceInfo &interface_info, const detail::StationInfo &station_info)
    {
        msg.connected = true;
        msg.ssid = interface_info.m_ssid.value_or("");
        msg.bssid = detail::network_utils::format_mac_address(station_info.m_mac_address);
        msg.frequency_mhz = interface_info.m_frequency_mhz.value_or(0);

        msg.signal_strength_dbm = station_info.m_signal_strength_dbm.value_or(std::numeric_limits<std::int8_t>::min());
        msg.signal_strength_average_dbm = station_info.m_signal_strength_average_dbm.value_or(std::numeric_limits<std::int8_t>::min());

        msg.tx_nominal_bitrate_mbps = static_cast<float>(station_info.m_tx_nominal_bitrate_mbps.value_or(std::numeric_limits<double>::quiet_NaN()));
        msg.rx_nominal_bitrate_mbps = static_cast<float>(station_info.m_rx_nominal_bitrate_mbps.value_or(std::numeric_limits<double>::quiet_NaN()));

        msg.tx_byte_count = station_info.m_tx_byte_count.value_or(0);
        msg.rx_byte_count = station_info.m_rx_byte_count.value_or(0);
        msg.tx_retry_count = station_info.m_tx_retry_count.value_or(0);
        msg.tx_failed_count = station_info.m_tx_failed_count.value_or(0);

        msg.tx_actual_bitrate_mbps = std::numeric_limits<float>::quiet_NaN();
        msg.rx_actual_bitrate_mbps = std::numeric_limits<float>::quiet_NaN();

        if (m_previous_sample && m_previous_sample->m_bssid == msg.bssid)
        {
            const auto elapsed_time = (rclcpp::Time(msg.header.stamp) - m_previous_sample->m_stamp).seconds();
            auto compute_bitrate_mbps = [elapsed_time](std::uint64_t previous_byte_count, std::uint64_t current_byte_count)
            {
                if (elapsed_time <= 0.0 || current_byte_count < previous_byte_count)
                {
                    return std::numeric_limits<float>::quiet_NaN();
                }
                return static_cast<float>((current_byte_count - previous_byte_count) * 8.0 / elapsed_time / 1e6);
            };
            msg.rx_actual_bitrate_mbps = compute_bitrate_mbps(m_previous_sample->m_rx_byte_count, msg.rx_byte_count);
            msg.tx_actual_bitrate_mbps = compute_bitrate_mbps(m_previous_sample->m_tx_byte_count, msg.tx_byte_count);
        }
        m_previous_sample = Sample{msg.header.stamp, msg.bssid, msg.rx_byte_count, msg.tx_byte_count};

        msg.inactive_for_ms = station_info.m_inactive_for_ms.value_or(0);
        msg.connected_for = station_info.m_connected_for.value_or(0);

        for (const auto &chain_info : station_info.m_chain_infos)
        {
            sfg_hardware_interface_msgs::msg::WifiAntennaStatus antenna_status;
            antenna_status.chain_index = chain_info.m_index;
            antenna_status.signal_strength_dbm = chain_info.m_signal_strength_dbm.value_or(std::numeric_limits<std::int8_t>::min());
            antenna_status.signal_strength_average_dbm = chain_info.m_signal_strength_average_dbm.value_or(std::numeric_limits<std::int8_t>::min());

            if (chain_info.m_index < m_antenna_frame_ids.size())
            {
                antenna_status.frame_id = m_antenna_frame_ids[chain_info.m_index];
            }
            else if (!m_antenna_frame_ids.empty())
            {
                RCLCPP_WARN_ONCE(get_logger(), "No frame ID specified for antenna with chain index %u.", chain_info.m_index);
            }
            msg.antennas.push_back(std::move(antenna_status));
        }
    }
}
