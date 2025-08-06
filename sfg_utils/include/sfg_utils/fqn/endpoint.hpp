#pragma once

namespace sfg_utils::fqn
{
    enum class Endpoint
    {
        ColorImageRaw,
        DepthImageRaw,
        ColorImageCompressed,
        DepthImageCompressed,
        CameraColorInfo,
        CameraDepthInfo,
        PointCloud,
        IMU,
        Custom
    };
}