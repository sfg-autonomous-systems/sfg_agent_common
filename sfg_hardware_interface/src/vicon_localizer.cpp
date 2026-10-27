#include "sfg_hardware_interface/vicon_localizer.hpp"

#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>

#include "sfg_utils/agent_utils.hpp"
#include "sfg_utils/ros_utils.hpp"

namespace sfg_hardware_interface
{
    namespace
    {
        template <typename ResponseType>
        const ResponseType &throw_on_failure(const ResponseType &response, const std::string &error)
        {
            if (response.Result != ViconDataStreamSDK::CPP::Result::Success)
            {
                throw std::runtime_error(error);
            }
            return response;
        }
    }

    ViconLocalizer::ViconLocalizer(const rclcpp::NodeOptions &options)
        : Node("localizer", rclcpp::NodeOptions(options).allow_undeclared_parameters(true).automatically_declare_parameters_from_overrides(true)),
          m_done(false),
          m_transform_buffer(std::make_unique<tf2_ros::Buffer>(get_clock())),
          m_transform_listener(std::make_unique<tf2_ros::TransformListener>(*m_transform_buffer)),
          m_tf_broadcaster(std::make_unique<tf2_ros::TransformBroadcaster>(this))
    {
        // Declare and retrieve ROS parameters.
        m_server_address = sfg_utils::ros_utils::declare_parameter_if_not_declared(
            *this,
            "server_address",
            "sfg-tracker:801",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The address (host:port) of the Vicon server."));

        m_buffer_size = sfg_utils::ros_utils::declare_parameter_if_not_declared(
            *this,
            "buffer_size",
            200,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("Number of frames the Vicon client should buffer internally."));

        m_subject_name = sfg_utils::ros_utils::declare_parameter_if_not_declared<std::string>(
            *this,
            "subject_name",
            sfg_utils::agent_utils::get_agent_name());

        m_map_frame_id = sfg_utils::ros_utils::declare_parameter_if_not_declared<std::string>(
            *this,
            "map_frame_id",
            "map");

        m_odom_frame_id = sfg_utils::ros_utils::declare_parameter_if_not_declared<std::string>(
            *this,
            "odom_frame_id",
            "odom");

        m_marker_frame_id = sfg_utils::ros_utils::declare_parameter_if_not_declared<std::string>(
            *this,
            "marker_frame_id",
            m_subject_name);

        m_base_link_frame_id = sfg_utils::ros_utils::declare_parameter_if_not_declared<std::string>(
            *this,
            "base_frame_id",
            m_subject_name);

        m_client_polling_thread = std::thread(&ViconLocalizer::poll_client, this);

        RCLCPP_INFO(get_logger(), "Started Vicon localizer for subject '%s', estimating '%s -> %s' from Vicon and odometry.", m_subject_name.c_str(), m_map_frame_id.c_str(), m_odom_frame_id.c_str());
    }

    ViconLocalizer::~ViconLocalizer()
    {
        m_done = true;

        if (m_client_polling_thread.joinable())
        {
            m_client_polling_thread.join();
        }
    }

    void ViconLocalizer::initialize_client()
    {
        RCLCPP_INFO(get_logger(), "Connecting to Vicon server at %s...", m_server_address.c_str());

        while (!m_done && rclcpp::ok() && !m_client.IsConnected().Connected)
        {
            m_client.Connect(m_server_address);

            if (m_client.IsConnected().Connected)
            {
                break;
            }

            if (!rclcpp::sleep_for(std::chrono::seconds(1)))
            {
                return;
            }
        }

        if (m_done || !rclcpp::ok())
        {
            return;
        }

        RCLCPP_INFO(get_logger(), "Connected to Vicon server at %s.", m_server_address.c_str());

        m_client.SetBufferSize(m_buffer_size);
        throw_on_failure(m_client.EnableSegmentData(), "Failed to enable segment data from Vicon server.");
        throw_on_failure(m_client.SetStreamMode(ViconDataStreamSDK::CPP::StreamMode::ServerPush), "Failed to set Vicon stream mode.");
    }

    void ViconLocalizer::poll_client()
    {
        try
        {
            initialize_client();
        }
        catch (const std::exception &exception)
        {
            RCLCPP_ERROR(get_logger(), "Failed to initialize Vicon client: %s", exception.what());
            return;
        }

        while (!m_done && rclcpp::ok() && m_client.IsConnected().Connected)
        {
            const auto frame_result = m_client.GetFrame().Result;

            if (frame_result == ViconDataStreamSDK::CPP::Result::NoFrame)
            {
                if (!rclcpp::sleep_for(std::chrono::milliseconds(10)))
                {
                    break;
                }
                continue;
            }
            
            if (frame_result != ViconDataStreamSDK::CPP::Result::Success)
            {
                throw std::runtime_error("Failed to retrieve a Vicon frame.");
            }

            try
            {
                process_frame();
            }
            catch (const std::exception &exception)
            {
                RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000, "Frame processing error: %s", exception.what());
            }
        }

        try
        {
            throw_on_failure(m_client.Disconnect(), "Failed to disconnect from Vicon server.");
        }
        catch (const std::exception &exception)
        {
            RCLCPP_WARN(get_logger(), "Failed to disconnect from Vicon server: %s", exception.what());
        }
    }

    void ViconLocalizer::process_frame()
    {
        auto stamp = now() - rclcpp::Duration::from_seconds(m_client.GetLatencyTotal().Total);
        auto segment_name = throw_on_failure(m_client.GetSubjectRootSegmentName(m_subject_name), "Failed to get subject root segment name from Vicon server.").SegmentName;
        auto segment_translation = throw_on_failure(m_client.GetSegmentGlobalTranslation(m_subject_name, segment_name), "Failed to get root segment global translation from Vicon server.");
        auto segment_rotation = throw_on_failure(m_client.GetSegmentGlobalRotationQuaternion(m_subject_name, segment_name), "Failed to get root segment global rotation from Vicon server.");

        if (segment_translation.Occluded || segment_rotation.Occluded)
        {
            return;
        }

        const auto millimeter_to_meter = 1.0 / 1000.0;
        auto map_to_marker_transform = tf2::Transform(
            tf2::Quaternion(segment_rotation.Rotation[0], segment_rotation.Rotation[1], segment_rotation.Rotation[2], segment_rotation.Rotation[3]),
            tf2::Vector3(segment_translation.Translation[0], segment_translation.Translation[1], segment_translation.Translation[2]) * millimeter_to_meter);

        tf2::Transform marker_to_base_link_transform;
        try
        {
            tf2::fromMsg(m_transform_buffer->lookupTransform(m_marker_frame_id, m_base_link_frame_id, stamp).transform, marker_to_base_link_transform);
        }
        catch (const tf2::TransformException &exception)
        {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000, "Failed to look up transform from marker frame ID '%s' to base frame ID '%s': %s", m_marker_frame_id.c_str(), m_base_link_frame_id.c_str(), exception.what());
            return;
        }

        tf2::Transform odom_to_base_link_transform;
        try
        {
            tf2::fromMsg(m_transform_buffer->lookupTransform(m_odom_frame_id, m_base_link_frame_id, stamp, std::chrono::milliseconds(50)).transform, odom_to_base_link_transform);
        }
        catch (const tf2::TransformException &exception)
        {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000, "Cannot look up odometry transform '%s' -> '%s': %s", m_odom_frame_id.c_str(), m_base_link_frame_id.c_str(), exception.what());
            return;
        }

        geometry_msgs::msg::TransformStamped msg;
        msg.header.stamp = stamp;
        msg.header.frame_id = m_map_frame_id;
        msg.child_frame_id = m_odom_frame_id;
        msg.transform = tf2::toMsg(map_to_marker_transform * marker_to_base_link_transform * odom_to_base_link_transform.inverse());
        m_tf_broadcaster->sendTransform(msg);
    }
}
