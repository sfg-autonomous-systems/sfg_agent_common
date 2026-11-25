#pragma once

#include <cstdint>
#include <optional>

namespace sfg_hardware_interface::detail
{
    struct ChainInfo
    {
    public:
        std::uint8_t m_index = 0;
        std::optional<std::int8_t> m_signal_strength_dbm;
        std::optional<std::int8_t> m_signal_strength_average_dbm;
    };
}