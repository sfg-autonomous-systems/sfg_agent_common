#include "sfg_vicon_localizer/localizer.hpp"

#include <chrono>
#include <limits>
#include <set>
#include <stdexcept>

#include <DataStreamClient.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>

#include "sfg_utils/agent_utils.hpp"
#include "sfg_utils/ros_utils.hpp"

namespace sfg_vicon_localizer
{
    struct Localizer::ViconClient
    {
        ViconDataStreamSDK::CPP::Client client;
    };

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

    Localizer::Localizer(const rclcpp::NodeOptions &options)
        : Node("localizer", rclcpp::NodeOptions(options).allow_undeclared_parameters(true).automatically_declare_parameters_from_overrides(true)),
          m_done(false),
          m_transform_buffer(std::make_unique<tf2_ros::Buffer>(get_clock())),
          m_transform_listener(std::make_unique<tf2_ros::TransformListener>(*m_transform_buffer)),
          m_tf_broadcaster(std::make_unique<tf2_ros::TransformBroadcaster>(this))
    {
        m_client = std::make_unique<ViconClient>();
        const auto agent_name = sfg_utils::agent_utils::get_agent_name();
        m_hostname = sfg_utils::ros_utils::declare_parameter_if_not_declared<std::string>(
            *this,
            "hostname",
            std::string("127.0.0.1:801"),
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The hostname and port of the Vicon server."));

        const auto buffer_size = sfg_utils::ros_utils::declare_parameter_if_not_declared<std::int64_t>(
            *this,
            "buffer_size",
            std::int64_t{200},
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("Number of frames the Vicon client should buffer internally."));
        if (buffer_size <= 0 ||
            buffer_size > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()))
        {
            throw std::invalid_argument("Parameter 'buffer_size' must be between 1 and " +
                                        std::to_string(std::numeric_limits<std::uint32_t>::max()) + ".");
        }
        m_buffer_size = static_cast<std::uint32_t>(buffer_size);

        m_subject_name = sfg_utils::ros_utils::declare_parameter_if_not_declared<std::string>(
            *this,
            "subject_name",
            agent_name);

        m_map_frame_id = sfg_utils::ros_utils::declare_parameter_if_not_declared<std::string>(
            *this,
            "global_frame_id",
            "map");

        m_odom_frame_id = sfg_utils::ros_utils::declare_parameter_if_not_declared<std::string>(
            *this,
            "odom_frame_id",
            "odom");

        m_object_to_frame_id_mapping = load_object_to_frame_id_mapping();
        m_client_polling_thread = std::thread(&Localizer::poll_client, this);

        RCLCPP_INFO(get_logger(), "Started Vicon localizer for subject '%s', estimating %s -> %s from Vicon and odometry.", m_subject_name.c_str(), m_map_frame_id.c_str(), m_odom_frame_id.c_str());
    }

    Localizer::~Localizer()
    {
        m_done = true;
        if (m_client_polling_thread.joinable())
        {
            m_client_polling_thread.join();
        }
    }

    std::unordered_map<std::string, Localizer::FrameIdMapping> Localizer::load_object_to_frame_id_mapping()
    {
        const std::string &object_to_frame_id_mapping_prefix = "object_to_frame_id_mapping";

        RCLCPP_INFO(get_logger(), "Loading object to frame ID mapping with mappings:");
        std::set<std::string> objects;

        for (const auto &parameter_name : list_parameters({object_to_frame_id_mapping_prefix}, 3).names)
        {
            // Parameter name is the full name including the prefix. We need to extract the object name by
            // removing the object to frame ID mapping prefix (and the dot) and...
            auto object = parameter_name.substr(object_to_frame_id_mapping_prefix.size() + 1);
            // ...removing any sub-parameter suffix (e.g. ".marker_frame_id" or ".base_frame_id").
            object = object.substr(0, object.find('.'));
            objects.insert(object);
        }

        std::unordered_map<std::string, FrameIdMapping> object_to_frame_id_mapping;

        for (const auto &object : objects)
        {
            auto parameter_prefix = object_to_frame_id_mapping_prefix + "." + object;
            auto marker_frame_id = get_parameter_or(parameter_prefix + ".marker_frame_id", object);
            auto base_frame_id = get_parameter_or(parameter_prefix + ".base_frame_id", marker_frame_id);
            object_to_frame_id_mapping[object] = {marker_frame_id, base_frame_id};

            RCLCPP_INFO(get_logger(), "  %s -> { marker frame ID: '%s', base frame ID: '%s' }", object.c_str(), marker_frame_id.c_str(), base_frame_id.c_str());
        }
        return object_to_frame_id_mapping;
    }

    void Localizer::initialize_client()
    {
        RCLCPP_INFO(get_logger(), "Connecting to Vicon server at %s...", m_hostname.c_str());

        while (!m_done && rclcpp::ok() && !m_client->client.IsConnected().Connected)
        {
            m_client->client.Connect(m_hostname);

            if (m_client->client.IsConnected().Connected)
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

        RCLCPP_INFO(get_logger(), "Connected to Vicon server at %s.", m_hostname.c_str());

        m_client->client.SetBufferSize(m_buffer_size);
        throw_on_failure(m_client->client.EnableSegmentData(), "Failed to enable segment data from Vicon server.");
        throw_on_failure(m_client->client.SetStreamMode(ViconDataStreamSDK::CPP::StreamMode::ServerPush), "Failed to set Vicon stream mode.");
    }

    void Localizer::poll_client()
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

        while (!m_done && rclcpp::ok() && m_client->client.IsConnected().Connected)
        {
            const auto frame_result = m_client->client.GetFrame().Result;
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
            throw_on_failure(m_client->client.Disconnect(), "Failed to disconnect from Vicon server.");
        }
        catch (const std::exception &exception)
        {
            RCLCPP_WARN(get_logger(), "Failed to disconnect from Vicon server: %s", exception.what());
        }
    }

    void Localizer::process_frame()
    {
        auto stamp = now() - rclcpp::Duration::from_seconds(m_client->client.GetLatencyTotal().Total);
        auto subject_count = throw_on_failure(m_client->client.GetSubjectCount(), "Failed to get subject count from Vicon server.").SubjectCount;

        for (size_t subject_index = 0; subject_index < subject_count; subject_index++)
        {
            // Vicon's DataStream SDK supports subjects having multiple segments. But since we are exclusively using Vicon Tracker as the server which
            // does not seem to support multi-segment subjects (i.e. tracked objects with multiple segments), we only retrieve the root segment for each subject.
            auto subject_name = throw_on_failure(m_client->client.GetSubjectName(subject_index), "Failed to get subject name from Vicon server.").SubjectName;
            auto segment_name = throw_on_failure(m_client->client.GetSubjectRootSegmentName(subject_name), "Failed to get subject root segment name from Vicon server.").SegmentName;
            auto segment_translation = throw_on_failure(m_client->client.GetSegmentGlobalTranslation(subject_name, segment_name), "Failed to get root segment global translation from Vicon server.");
            auto segment_rotation = throw_on_failure(m_client->client.GetSegmentGlobalRotationQuaternion(subject_name, segment_name), "Failed to get root segment global rotation from Vicon server.");

            if (segment_translation.Occluded || segment_rotation.Occluded)
            {
                continue;
            }
            const auto millimeter_to_meter = 1.0 / 1000.0;

            auto map_to_marker = tf2::Transform(
                tf2::Quaternion(segment_rotation.Rotation[0], segment_rotation.Rotation[1], segment_rotation.Rotation[2], segment_rotation.Rotation[3]),
                tf2::Vector3(segment_translation.Translation[0], segment_translation.Translation[1], segment_translation.Translation[2]) * millimeter_to_meter);

            auto frame_id_mapping = get_frame_id_mapping(subject_name);

            tf2::Transform marker_to_base;
            try
            {
                const auto marker_to_base_message = m_transform_buffer->lookupTransform(
                    frame_id_mapping.m_marker_frame_id,
                    frame_id_mapping.m_base_frame_id,
                    stamp);
                tf2::fromMsg(marker_to_base_message.transform, marker_to_base);
            }
            catch (const tf2::TransformException &exception)
            {
                RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000, "Failed to look up transform from marker frame ID '%s' to base frame ID '%s': %s", frame_id_mapping.m_marker_frame_id.c_str(), frame_id_mapping.m_base_frame_id.c_str(), exception.what());
                continue;
            }

            tf2::Transform odom_to_base;
            try
            {
                const auto odom_to_base_message = m_transform_buffer->lookupTransform(
                    m_odom_frame_id,
                    m_base_frame_id,
                    stamp,
                    std::chrono::milliseconds(50));
                tf2::fromMsg(odom_to_base_message.transform, odom_to_base);
            }
            catch (const tf2::TransformException &exception)
            {
                RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000, "Cannot look up odometry transform '%s' -> '%s': %s", m_odom_frame_id.c_str(), m_base_frame_id.c_str(), exception.what());
                continue;
            }

            geometry_msgs::msg::TransformStamped map_to_odom_message;
            map_to_odom_message.header.stamp = stamp;
            map_to_odom_message.header.frame_id = m_map_frame_id;
            map_to_odom_message.child_frame_id = m_odom_frame_id;
            map_to_odom_message.transform = tf2::toMsg(map_to_marker * marker_to_base * odom_to_base.inverse());
            m_tf_broadcaster->sendTransform(map_to_odom_message);
        }
    }

    Localizer::FrameIdMapping Localizer::get_frame_id_mapping(const std::string &object_name) const
    {
        if (auto iterator = m_object_to_frame_id_mapping.find(object_name); iterator != m_object_to_frame_id_mapping.end())
        {
            return iterator->second;
        }
        return {object_name, object_name};
    }
}
