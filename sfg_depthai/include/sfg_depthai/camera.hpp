#pragma once

#include <depthai/depthai.hpp>
#include <depthai_bridge/ImageConverter.hpp>
#include <image_transport/image_transport.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/camera_info.hpp>

namespace sfg_depthai
{
    class Camera : public rclcpp::Node
    {
    public:
        Camera(const rclcpp::NodeOptions &options);

    private:
        struct CameraStream
        {
            sensor_msgs::msg::CameraInfo m_camera_info;
            std::unique_ptr<depthai_bridge::ImageConverter> m_image_converter;
            image_transport::CameraPublisher m_publisher;
        };

        void setup_pipeline();
        void start_pipeline();
        void callback(const std::shared_ptr<dai::ADatatype> &data);
        std::pair<int, int> parse_resolution(const std::string &resolution);

        static constexpr auto s_color_socket = dai::CameraBoardSocket::CAM_A;
        static constexpr auto s_left_socket = dai::CameraBoardSocket::CAM_B;
        static constexpr auto s_right_socket = dai::CameraBoardSocket::CAM_C;

        // ROS parameters
        std::pair<int, int> m_color_resolution;
        std::pair<int, int> m_depth_resolution;
        int m_fps;
        std::string m_frame_id;

        dai::Pipeline m_pipeline;
        std::shared_ptr<dai::Device> m_device;
        std::shared_ptr<dai::MessageQueue> m_output_queue;

        CameraStream m_color_stream;
        CameraStream m_depth_stream;
    };
}