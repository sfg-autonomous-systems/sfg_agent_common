#include "sfg_depthai/camera.hpp"

#include "sfg_utils/fqn/ros_fqn_builder.hpp"

namespace sfg_depthai
{
    Camera::Camera(const rclcpp::NodeOptions &options) : Node("camera", options)
    {
        // Declare and retrieve ROS parameters.
        m_color_resolution = parse_resolution(declare_parameter(
            "color.resolution",
            "1920x1080",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The resolution of the color camera.")));

        m_depth_resolution = parse_resolution(declare_parameter(
            "depth.resolution",
            "1280x720",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The resolution of the depth camera.")));

        m_fps = declare_parameter(
            "fps",
            15,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The fps of the camera.")
                .set__integer_range(
                    {rcl_interfaces::msg::IntegerRange()
                         .set__from_value(1)
                         .set__to_value(60)}));

        m_frame_id = declare_parameter(
            "frame_id",
            std::string(get_name()) + "_color_optical_frame",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The frame id of the published color and depth images."));

        using namespace sfg_utils::fqn;

        // Set up interfaces.
        m_color_stream.m_publisher = image_transport::create_camera_publisher(
            this,
            RosFqnBuilder().stream(Stream::Color).resource(Resource::ImageRaw).build(RosFqnSegment::Stream, RosFqnSegment::Resource),
            rmw_qos_profile_sensor_data);
        m_depth_stream.m_publisher = image_transport::create_camera_publisher(
            this,
            RosFqnBuilder().stream(Stream::Depth).resource(Resource::ImageRaw).build(RosFqnSegment::Stream, RosFqnSegment::Resource),
            rmw_qos_profile_sensor_data);

        setup_pipeline();
        start_pipeline();

        RCLCPP_INFO(this->get_logger(), "Started camera with color resolution %dx%d, depth resolution %dx%d, and fps %d.", m_color_resolution.first, m_color_resolution.second, m_depth_resolution.first, m_depth_resolution.second, m_fps);
    }

    void Camera::setup_pipeline()
    {
        auto color = m_pipeline.create<dai::node::Camera>()->build(s_color_socket);
        auto left = m_pipeline.create<dai::node::Camera>()->build(s_left_socket);
        auto right = m_pipeline.create<dai::node::Camera>()->build(s_right_socket);
        auto stereo = m_pipeline.create<dai::node::StereoDepth>();
        auto sync = m_pipeline.create<dai::node::Sync>();

        auto color_out = color->requestOutput(m_color_resolution, std::nullopt, dai::ImgResizeMode::CROP, m_fps, true);
        auto left_out = left->requestOutput(m_depth_resolution, std::nullopt, dai::ImgResizeMode::CROP, m_fps);
        auto right_out = right->requestOutput(m_depth_resolution, std::nullopt, dai::ImgResizeMode::CROP, m_fps);

        stereo->setDefaultProfilePreset(dai::node::StereoDepth::PresetMode::DEFAULT);
        stereo->setLeftRightCheck(true);
        stereo->setRectification(true);
        stereo->setOutputSize(m_depth_resolution.first, m_depth_resolution.second);

        sync->setSyncThreshold(std::chrono::milliseconds(static_cast<std::int32_t>((1.0f / m_fps) * 1000.0f * 0.5f)));

        // Link the left and right mono camera outputs to the stereo inputs.
        left_out->link(stereo->left);
        right_out->link(stereo->right);

        // Link the color camera output to the stereo input to align to.
        color_out->link(stereo->inputAlignTo);

        // Link the color and stereo outputs to the sync inputs so that we get synchronized frames.
        color_out->link(sync->inputs["color"]);
        stereo->depth.link(sync->inputs["depth"]);

        // Create an output queue to receive the synchronized frames.
        m_output_queue = sync->out.createOutputQueue();
        m_output_queue->addCallback(std::bind(&Camera::callback, this, std::placeholders::_1));
    }

    void Camera::start_pipeline()
    {
        m_device = m_pipeline.getDefaultDevice();

        m_color_stream.m_image_converter = std::make_unique<depthai_bridge::ImageConverter>(m_frame_id, false, false);
        m_color_stream.m_camera_info = m_color_stream.m_image_converter->calibrationToCameraInfo(m_device->readCalibration(), s_color_socket, m_color_resolution.first, m_color_resolution.second);

        m_depth_stream.m_image_converter = std::make_unique<depthai_bridge::ImageConverter>(m_frame_id, false, false);
        m_depth_stream.m_camera_info = m_depth_stream.m_image_converter->calibrationToCameraInfo(m_device->readCalibration(), s_color_socket, m_depth_resolution.first, m_depth_resolution.second);

        m_pipeline.start();
    }

    void Camera::callback(const std::shared_ptr<dai::ADatatype> &data)
    {
        auto message_group = std::dynamic_pointer_cast<dai::MessageGroup>(data);
        std::array<const char *, 2> frames = {"color", "depth"};
        std::array<CameraStream *, 2> streams = {&m_color_stream, &m_depth_stream};

        for (size_t index = 0; index < streams.size(); index++)
        {
            auto frame = message_group->get<dai::ImgFrame>(frames[index]);
            const auto &stream = streams[index];

            sensor_msgs::msg::Image::SharedPtr image_msg = stream->m_image_converter->toRosMsgPtr(frame);
            auto camera_info_msg = std::make_shared<sensor_msgs::msg::CameraInfo>(stream->m_camera_info);
            camera_info_msg->header = image_msg->header;
            camera_info_msg->width = image_msg->width;
            camera_info_msg->height = image_msg->height;
            stream->m_publisher.publish(image_msg, camera_info_msg);
        }
    }

    std::pair<int, int> Camera::parse_resolution(const std::string &resolution)
    {
        auto x_position = resolution.find('x');

        if (x_position == std::string::npos)
        {
            throw std::invalid_argument("Invalid resolution format. Expected format is <width>x<height>.");
        }

        try
        {
            return {std::stoi(resolution.substr(0, x_position)), std::stoi(resolution.substr(x_position + 1))};
        }
        catch (const std::exception &)
        {
            throw std::invalid_argument("Invalid resolution format. Width and height should be integers.");
        }
    }
}