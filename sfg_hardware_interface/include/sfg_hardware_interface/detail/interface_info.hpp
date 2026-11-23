#pragma once

#include <cstdint>
#include <linux/nl80211.h>
#include <optional>
#include <string>

namespace sfg_hardware_interface::detail
{
    struct InterfaceInfo
    {
    public:
        std::string m_interface_name;
        size_t m_interface_index = 0;
        uint32_t m_interface_type = NL80211_IFTYPE_UNSPECIFIED;
        std::optional<std::string> m_ssid;
        std::optional<std::string> m_bssid;
        std::optional<uint32_t> m_frequency_mhz;
    };
}
