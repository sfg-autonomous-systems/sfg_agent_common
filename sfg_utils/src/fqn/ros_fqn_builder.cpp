#include "sfg_utils/fqn/ros_fqn_builder.hpp"

#include <sstream>
#include <stdexcept>
#include <vector>

#include "sfg_utils/agent_utils.hpp"

namespace sfg_utils::fqn
{
    RosFQNBuilder &RosFQNBuilder::scope(Scope scope)
    {
        switch (scope)
        {
        case Scope::Global:
            m_scope = "global";
            break;
        case Scope::Local:
            m_scope = "local";
            break;
        }

        return *this;
    }

    RosFQNBuilder &RosFQNBuilder::agent(const std::string &name)
    {
        m_agent = sanitize_agent_name(name.empty() ? get_agent_name() : name);
        return *this;
    }

    RosFQNBuilder &RosFQNBuilder::component(Component type, const std::string &name)
    {
        switch (type)
        {
        case Component::Camera:
            m_component = "camera_" + name;
            break;
        case Component::Lidar:
            m_component = "lidar_" + name;
            break;
        case Component::IMU:
            m_component = "imu_" + name;
            break;
        case Component::Custom:
            m_component = name;
            break;
        }

        return *this;
    }

    RosFQNBuilder &RosFQNBuilder::endpoint(Endpoint type, const std::string &stream, const std::string &resource)
    {
        switch (type)
        {
        case Endpoint::ColorImageRaw:
            m_stream = "color";
            m_resource = "image_raw";
            break;
        case Endpoint::DepthImageRaw:
            m_stream = "depth";
            m_resource = "image_raw";
            break;
        case Endpoint::ColorImageCompressed:
            m_stream = "color";
            m_resource = "image_compressed";
            break;
        case Endpoint::DepthImageCompressed:
            m_stream = "depth";
            m_resource = "image_compressed";
            break;
        case Endpoint::CameraColorInfo:
            m_stream = "color";
            m_resource = "camera_info";
            break;
        case Endpoint::CameraDepthInfo:
            m_stream = "depth";
            m_resource = "camera_info";
            break;
        case Endpoint::PointCloud:
            m_stream = "points";
            m_resource.reset();
            break;
        case Endpoint::IMU:
            m_stream = "imu";
            m_resource.reset();
            break;
        case Endpoint::Custom:
            if (stream.empty())
            {
                throw std::invalid_argument("Parameter 'stream' is required for Custom endpoint.");
            }

            m_stream = stream;

            if (resource.empty())
            {
                m_resource.reset();
            }
            else
            {
                m_resource = resource;
            }
            break;
        }

        return *this;
    }

    [[nodiscard]] std::string RosFQNBuilder::build(bool only_namespace) const
    {
        std::stringstream result;

        if (!m_scope.has_value())
        {
            throw std::logic_error("FQN build failed: Scope was not set.");
        }

        result << "/" << *m_scope;

        if (m_agent.has_value())
        {
            result << "/" << *m_agent;
        }

        if (only_namespace)
        {
            return result.str();
        }

        if (m_component.has_value() && !m_agent.has_value())
        {
            throw std::logic_error("FQN build failed: Component was set without an agent.");
        }

        if (!m_stream.has_value())
        {
            throw std::logic_error("FQN build failed: Endpoint was not set.");
        }

        if (m_component.has_value())
        {
            result << "/" << *m_component;
        }

        result << "/" << *m_stream;

        if (m_resource.has_value())
        {
            result << "/" << *m_resource;
        }

        return result.str();
    }
}
