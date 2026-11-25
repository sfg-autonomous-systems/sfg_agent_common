#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <rclcpp/rclcpp.hpp>
#include <sfg_hardware_interface_msgs/msg/wifi_link_status.hpp>
#include <string>
#include <vector>

// Forward declare the client so we don't need libnl as a public dependency.
namespace sfg_hardware_interface::detail
{
    struct InterfaceInfo;
    class Nl80211Client;
    struct StationInfo;
}

namespace sfg_hardware_interface
{
    class WifiLinkStatusPublisher : public rclcpp::Node
    {
    public:
        WifiLinkStatusPublisher(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
        WifiLinkStatusPublisher(const WifiLinkStatusPublisher &) = delete;
        WifiLinkStatusPublisher &operator=(const WifiLinkStatusPublisher &) = delete;
        WifiLinkStatusPublisher(WifiLinkStatusPublisher &&) = delete;
        WifiLinkStatusPublisher &operator=(WifiLinkStatusPublisher &&) = delete;
        ~WifiLinkStatusPublisher() override;

    private:
        struct Sample
        {
        public:
            rclcpp::Time m_stamp;
            std::string m_bssid;
            std::uint64_t m_rx_byte_count;
            std::uint64_t m_tx_byte_count;
        };

        void publish_link_status();

        void validate_client(detail::Nl80211Client &client);
        void fill_link_status(sfg_hardware_interface_msgs::msg::WifiLinkStatus &msg, const detail::InterfaceInfo &interface_info, const detail::StationInfo &station_info);

        // ROS parameters
        std::string m_network_interface;
        std::chrono::milliseconds m_receive_timeout_ms;
        float m_publish_rate;
        std::string m_frame_id;
        std::vector<std::string> m_antenna_frame_ids;

        std::unique_ptr<detail::Nl80211Client> m_nl80211_client;
        std::optional<Sample> m_previous_sample;

        rclcpp::Publisher<sfg_hardware_interface_msgs::msg::WifiLinkStatus>::SharedPtr m_link_status_publisher;
        rclcpp::TimerBase::SharedPtr m_timer;
    };
}
