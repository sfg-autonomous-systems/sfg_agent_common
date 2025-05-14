#include "sfg_depthai/camera.hpp"

namespace sfg_depthai
{
    Camera::Camera(const rclcpp::NodeOptions &options)
        : Node("camera", options)
    {
        // Declare and retrieve ROS parameters.
        std::string parameter = "color_resolution";
        declare_parameter(
            parameter,
            "1080p",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The resolution of the color camera.")
                .set__additional_constraints(
                    "Valid values are: 1080p, 4k, 12mp, 13mp, 720p, 800p, 1200p"));
        std::string color_resolution;
        get_parameter(parameter, color_resolution);

        if (!parse_color_resolution(color_resolution, m_color_resolution))
        {
            throw std::runtime_error("Invalid color resolution");
        }

        parameter = "depth_resolution";
        declare_parameter(
            parameter,
            "720p",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The resolution of the depth camera.")
                .set__additional_constraints(
                    "Valid values are: 720p, 800p, 400p, 480p, 1200p"));
        std::string depth_resolution;
        get_parameter(parameter, depth_resolution);

        if (!parse_depth_resolution(depth_resolution, m_depth_resolution))
        {
            throw std::runtime_error("Invalid depth resolution");
        }

        parameter = "fps";
        declare_parameter(
            parameter,
            15,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The fps of the camera.")
                .set__integer_range(
                    {rcl_interfaces::msg::IntegerRange()
                         .set__from_value(1)
                         .set__to_value(60)}));
        get_parameter(parameter, m_fps);

        parameter = "frame_id";
        declare_parameter(
            parameter,
            "camera",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The frame id of the camera."));
        get_parameter(parameter, m_frame_id);

        setup_device();

        // Set up interfaces.
        m_color_publisher = image_transport::create_camera_publisher(this, m_frame_id + "/color/image_raw");
        m_depth_publisher = image_transport::create_camera_publisher(this, m_frame_id + "/depth/image_raw");
        m_output_queue->addCallback(std::bind(&Camera::callback, this, std::placeholders::_1));

        RCLCPP_INFO(this->get_logger(), "Started camera.");
    }

    void Camera::setup_device()
    {
        constexpr auto COLOR_SOCKET = dai::CameraBoardSocket::CAM_A;
        constexpr auto LEFT_SOCKET = dai::CameraBoardSocket::CAM_B;
        constexpr auto RIGHT_SOCKET = dai::CameraBoardSocket::CAM_C;

        auto color = m_pipeline.create<dai::node::ColorCamera>();
        auto left = m_pipeline.create<dai::node::MonoCamera>();
        auto right = m_pipeline.create<dai::node::MonoCamera>();
        auto depth = m_pipeline.create<dai::node::StereoDepth>();
        auto sync = m_pipeline.create<dai::node::Sync>();
        auto out = m_pipeline.create<dai::node::XLinkOut>();
        auto align = m_pipeline.create<dai::node::ImageAlign>();

        left->setBoardSocket(LEFT_SOCKET);
        left->setResolution(m_depth_resolution);
        left->setFps(m_fps);

        right->setBoardSocket(RIGHT_SOCKET);
        right->setResolution(m_depth_resolution);
        right->setFps(m_fps);

        color->setBoardSocket(COLOR_SOCKET);
        color->setResolution(m_color_resolution);
        color->setFps(m_fps);
        color->setInterleaved(false);

        depth->setDefaultProfilePreset(dai::node::StereoDepth::PresetMode::DEFAULT);
        depth->setDepthAlign(LEFT_SOCKET);
        depth->setLeftRightCheck(true);
        depth->setSubpixel(true);

        out->setStreamName("out");

        sync->setSyncThreshold(std::chrono::milliseconds(static_cast<int>((1.0f / m_fps) * 1000.0 * 0.5)));

        color->isp.link(sync->inputs["rgb"]);
        left->out.link(depth->left);
        right->out.link(depth->right);
        depth->depth.link(align->input);
        align->outputAligned.link(sync->inputs["depth_aligned"]);
        color->isp.link(align->inputAlignTo);
        sync->out.link(out->input);

        m_device = std::make_unique<dai::Device>(m_pipeline);
        m_output_queue = m_device->getOutputQueue("out", 8, false);

        m_color_converter = std::make_unique<dai::ros::ImageConverter>(m_frame_id, false, true);
        m_depth_converter = std::make_unique<dai::ros::ImageConverter>(m_frame_id, false, true);

        auto color_width = color->getVideoWidth();
        auto color_height = color->getVideoHeight();
        dai::CalibrationHandler calibration = m_device->readCalibration();
        m_color_camera_info = m_color_converter->calibrationToCameraInfo(calibration, COLOR_SOCKET, color_width, color_height);

        // Technically this is redundant, but if we ever change the alignment, the socket
        // will change too, so we would need to get the calibration for the new socket.
        auto depth_width = color_width;
        auto depth_height = color_width;
        calibration = m_device->readCalibration();
        m_depth_camera_info = m_depth_converter->calibrationToCameraInfo(calibration, COLOR_SOCKET, depth_width, depth_height);
    }

    void Camera::callback(const std::shared_ptr<dai::ADatatype> &data)
    {
        auto message_group = std::dynamic_pointer_cast<dai::MessageGroup>(data);
        auto color = message_group->get<dai::ImgFrame>("rgb");
        auto depth = message_group->get<dai::ImgFrame>("depth_aligned");

        if (!color || !depth)
        {
            RCLCPP_ERROR(this->get_logger(), "Failed to get color or depth frame.");
            return;
        }

        sensor_msgs::msg::Image::SharedPtr image_msg = m_color_converter->toRosMsgPtr(color);
        auto camera_info_msg = std::make_shared<sensor_msgs::msg::CameraInfo>(m_color_camera_info);
        camera_info_msg->header = image_msg->header;
        m_color_publisher.publish(image_msg, camera_info_msg);

        image_msg = m_depth_converter->toRosMsgPtr(depth);
        camera_info_msg = std::make_shared<sensor_msgs::msg::CameraInfo>(m_depth_camera_info);
        camera_info_msg->header = image_msg->header;
        m_depth_publisher.publish(image_msg, camera_info_msg);
    }

    bool Camera::parse_color_resolution(
        const std::string &resolution,
        dai::ColorCameraProperties::SensorResolution &colorResolution)
    {
        if (resolution == "1080p")
        {
            colorResolution = dai::ColorCameraProperties::SensorResolution::THE_1080_P;
            return true;
        }
        else if (resolution == "4k")
        {
            colorResolution = dai::ColorCameraProperties::SensorResolution::THE_4_K;
            return true;
        }
        else if (resolution == "12mp")
        {
            colorResolution = dai::ColorCameraProperties::SensorResolution::THE_12_MP;
            return true;
        }
        else if (resolution == "13mp")
        {
            colorResolution = dai::ColorCameraProperties::SensorResolution::THE_13_MP;
            return true;
        }
        else if (resolution == "720p")
        {
            colorResolution = dai::ColorCameraProperties::SensorResolution::THE_720_P;
            return true;
        }
        else if (resolution == "800p")
        {
            colorResolution = dai::ColorCameraProperties::SensorResolution::THE_800_P;
            return true;
        }
        else if (resolution == "1200p")
        {
            colorResolution = dai::ColorCameraProperties::SensorResolution::THE_1200_P;
            return true;
        }

        return false;
    }

    bool Camera::parse_depth_resolution(
        const std::string &resolution,
        dai::MonoCameraProperties::SensorResolution &monoResolution)
    {
        if (resolution == "720p")
        {
            monoResolution = dai::MonoCameraProperties::SensorResolution::THE_720_P;
            return true;
        }
        else if (resolution == "800p")
        {
            monoResolution = dai::MonoCameraProperties::SensorResolution::THE_800_P;
            return true;
        }
        else if (resolution == "400p")
        {
            monoResolution = dai::MonoCameraProperties::SensorResolution::THE_400_P;
            return true;
        }
        else if (resolution == "480p")
        {
            monoResolution = dai::MonoCameraProperties::SensorResolution::THE_480_P;
            return true;
        }
        else if (resolution == "1200p")
        {
            monoResolution = dai::MonoCameraProperties::SensorResolution::THE_1200_P;
            return true;
        }

        return false;
    }
}