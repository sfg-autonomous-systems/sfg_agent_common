#pragma once

#include <cctype>
#include <magic_enum_flags.hpp>

namespace sfg_utils::fqn
{
    enum class Resource : std::uint64_t
    {
        None = 0,
        ImageRaw = 1 << 0,
        ImageCompressed = 1 << 1,
        CameraInfo = 1 << 2,
        RobotDescription = 1 << 3,
        PointCloud = 1 << 4,
        IMU = 1 << 5,
        AgentHeartbeat = 1 << 6,
        Custom = 1 << 7,
        All = ImageRaw | ImageCompressed | CameraInfo | RobotDescription | PointCloud | IMU | AgentHeartbeat | Custom
    };
}

template <>
struct magic_enum::customize::enum_range<sfg_utils::fqn::Resource>
{
    static constexpr bool is_flags = true;
};