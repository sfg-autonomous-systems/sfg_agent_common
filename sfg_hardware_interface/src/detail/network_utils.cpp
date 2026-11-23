#include "sfg_hardware_interface/detail/network_utils.hpp"

#include <cstdio>

namespace sfg_hardware_interface::detail::network_utils
{
    std::string format_mac_address(const std::array<std::uint8_t, ETH_ALEN> &mac_address)
    {
        char buffer[18];
        std::snprintf(
            buffer,
            sizeof(buffer), "%02x:%02x:%02x:%02x:%02x:%02x",
            mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5]);
        return std::string(buffer);
    }
}