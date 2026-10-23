#pragma once

#include <atomic>
#include <cstdint>
#include <DataStreamClient.h>
#include <memory>
#include <rclcpp/rclcpp.hpp>
#include <string>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_broadcaster.hpp>
#include <tf2_ros/transform_listener.h>
#include <thread>

namespace sfg_hardware_interface
{
    class Localizer final : public rclcpp::Node
    {
    public:
        Localizer(const rclcpp::NodeOptions &options = rclcpp::NodeOptions());
        Localizer(const Localizer &) = delete;
        Localizer &operator=(const Localizer &) = delete;
        Localizer(Localizer &&) = delete;
        Localizer &operator=(Localizer &&) = delete;
        ~Localizer() override;

    private:
        void initialize_client();
        void poll_client();
        void process_frame();

        std::string m_hostname;
        std::string m_subject_name;
        std::string m_map_frame_id;
        std::string m_odom_frame_id;
        std::string m_marker_frame_id;
        std::string m_base_frame_id;
        std::uint32_t m_buffer_size;

        ViconDataStreamSDK::CPP::Client m_client;
        std::thread m_client_polling_thread;
        std::atomic<bool> m_done;

        std::unique_ptr<tf2_ros::Buffer> m_transform_buffer;
        std::unique_ptr<tf2_ros::TransformListener> m_transform_listener;
        std::unique_ptr<tf2_ros::TransformBroadcaster> m_tf_broadcaster;
    };
}
