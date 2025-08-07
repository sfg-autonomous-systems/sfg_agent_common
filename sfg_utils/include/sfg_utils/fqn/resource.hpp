#pragma once

namespace sfg_utils::fqn
{
    enum class Resource
    {
        ImageRaw,
        ImageCompressed,
        CameraInfo,
        RobotDescription,
        PointCloud,
        IMU,
        Custom
    };
}