#pragma once

#include <cctype>
#include <magic_enum_flags.hpp>

namespace sfg_utils::fqn
{
    enum class Resource : std::uint64_t
    {
        AgentHeartbeat = 1 << 1,
        CameraInfo = 1 << 2,
        CmdVel = 1 << 3,
        ImageCompressed = 1 << 4,
        ImageRaw = 1 << 5,
        Imu = 1 << 6,
        JointStates = 1 << 7,
        PointCloud = 1 << 8,
        RobotDescription = 1 << 9,
        TriggerAction = 1 << 10,

        None = 0,
        Custom = 1,
        All = ~None
    };
}

template <>
struct magic_enum::customize::enum_range<sfg_utils::fqn::Resource>
{
    static constexpr auto is_flags = true;
};