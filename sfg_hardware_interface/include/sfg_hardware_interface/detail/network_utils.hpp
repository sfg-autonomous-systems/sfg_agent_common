#pragma once

#include <array>
#include <cstdint>
#include <linux/if_ether.h>
#include <string>

namespace sfg_hardware_interface::detail::network_utils
{
    std::string format_mac_address(const std::array<std::uint8_t, ETH_ALEN> &mac_address);
}
