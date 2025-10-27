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
            std::unique_ptr<dai::ros::ImageConverter> m_image_converter;
            image_transport::CameraPublisher m_publisher;
        };

        void setup_device();
        void callback(const std::shared_ptr<dai::ADatatype> &data);
        dai::ColorCameraProperties::SensorResolution parse_color_resolution(const std::string &resolution);
        dai::MonoCameraProperties::SensorResolution parse_depth_resolution(const std::string &resolution);

        // ROS parameters
        dai::ColorCameraProperties::SensorResolution m_color_resolution;
        dai::MonoCameraProperties::SensorResolution m_depth_resolution;
        int m_fps;
        std::string m_frame_id;

        dai::Pipeline m_pipeline;
        std::unique_ptr<dai::Device> m_device;
        std::shared_ptr<dai::DataOutputQueue> m_output_queue;

        CameraStream m_color_stream;
        CameraStream m_depth_stream;
    };
}