#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include <rclcpp/rclcpp.hpp>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_broadcaster.hpp>
#include <tf2_ros/transform_listener.h>

namespace sfg_vicon_localizer
{
    class Localizer : public rclcpp::Node
    {
    public:
        explicit Localizer(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
        Localizer(const Localizer &) = delete;
        Localizer &operator=(const Localizer &) = delete;
        Localizer(Localizer &&) = delete;
        Localizer &operator=(Localizer &&) = delete;
        ~Localizer() override;

    private:
        struct ViconClient;

        struct FrameIdMapping
        {
        public:
            std::string m_marker_frame_id;
            std::string m_base_frame_id;
        };
        std::unordered_map<std::string, FrameIdMapping> load_object_to_frame_id_mapping();
        void initialize_client();
        void poll_client();
        void process_frame();
        FrameIdMapping get_frame_id_mapping(const std::string &object_name) const;

        // ROS parameters
        std::string m_hostname;
        std::string m_subject_name;
        std::string m_map_frame_id;
        std::string m_odom_frame_id;
        std::string m_marker_frame_id;
        std::string m_base_frame_id;
        std::uint32_t m_buffer_size;

        std::unordered_map<std::string, FrameIdMapping> m_object_to_frame_id_mapping;
        std::unique_ptr<ViconClient> m_client;
        std::thread m_client_polling_thread;
        std::atomic<bool> m_done;

        std::unique_ptr<tf2_ros::Buffer> m_transform_buffer;
        std::unique_ptr<tf2_ros::TransformListener> m_transform_listener;
        std::unique_ptr<tf2_ros::TransformBroadcaster> m_tf_broadcaster;
    };
}
