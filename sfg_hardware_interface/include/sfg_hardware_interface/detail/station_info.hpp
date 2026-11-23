#pragma once

#include <array>
#include <cstdint>
#include <linux/if_ether.h>
#include <optional>

namespace sfg_hardware_interface::detail
{
    struct StationInfo
    {
    public:
        std::array<std::uint8_t, ETH_ALEN> m_mac_address{};
        std::optional<std::int32_t> m_signal_strength_dbm;
        std::optional<std::int32_t> m_signal_strength_average_dbm;
        std::optional<double> m_tx_nominal_bitrate_mbps;
        std::optional<double> m_rx_nominal_bitrate_mbps;
        std::optional<std::uint64_t> m_rx_byte_count;
        std::optional<std::uint64_t> m_tx_byte_count;
        std::optional<std::uint32_t> m_tx_retry_count;
        std::optional<std::uint32_t> m_tx_failed_count;
        std::optional<std::uint32_t> m_inactive_for_ms;
        std::optional<std::uint32_t> m_connected_for_ms;
    };
}
