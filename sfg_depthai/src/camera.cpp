#include "sfg_depthai/camera.hpp"

#include "sfg_utils/fqn/ros_fqn_builder.hpp"

namespace sfg_depthai
{
    const std::map<std::string, dai::ColorCameraProperties::SensorResolution> Camera::s_color_resolution_map = {
        {"1280x720", dai::ColorCameraProperties::SensorResolution::THE_720_P},
        {"1280x800", dai::ColorCameraProperties::SensorResolution::THE_800_P},
        {"1352x1012", dai::ColorCameraProperties::SensorResolution::THE_1352X1012},
        {"1440x1080", dai::ColorCameraProperties::SensorResolution::THE_1440X1080},
        {"1920x1080", dai::ColorCameraProperties::SensorResolution::THE_1080_P},
        {"1920x1200", dai::ColorCameraProperties::SensorResolution::THE_1200_P},
        {"2024x1520", dai::ColorCameraProperties::SensorResolution::THE_2024X1520},
        {"2592x1944", dai::ColorCameraProperties::SensorResolution::THE_5_MP},
        {"4000x3000", dai::ColorCameraProperties::SensorResolution::THE_4000X3000},
        {"4056x3040", dai::ColorCameraProperties::SensorResolution::THE_12_MP},
        {"4208x3120", dai::ColorCameraProperties::SensorResolution::THE_13_MP},
        {"3840x2160", dai::ColorCameraProperties::SensorResolution::THE_4_K},
        {"5312x6000", dai::ColorCameraProperties::SensorResolution::THE_5312X6000},
        {"8000x6000", dai::ColorCameraProperties::SensorResolution::THE_48_MP}};

    const std::map<std::string, dai::MonoCameraProperties::SensorResolution> Camera::s_depth_resolution_map = {
        {"640x400", dai::MonoCameraProperties::SensorResolution::THE_400_P},
        {"640x480", dai::MonoCameraProperties::SensorResolution::THE_480_P},
        {"1280x720", dai::MonoCameraProperties::SensorResolution::THE_720_P},
        {"1280x800", dai::MonoCameraProperties::SensorResolution::THE_800_P},
        {"1920x1200", dai::MonoCameraProperties::SensorResolution::THE_1200_P}};

    template <typename MapType>
    std::string Camera::get_available_resolutions(MapType resolution_map)
    {
        if (resolution_map.empty())
        {
            return "None";
        }
        std::stringstream resolutions;

        for (const auto &pair : resolution_map)
        {
            resolutions << pair.first << ", ";
        }
        return resolutions.str().erase(resolutions.str().length() - 2);
    }

    Camera::Camera(const rclcpp::NodeOptions &options)
        : Node("camera", options)
    {
        // Declare and retrieve ROS parameters.
        m_color_resolution = parse_color_resolution(declare_parameter(
            "color.resolution",
            "1920x1080",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The resolution of the color camera.")
                .set__additional_constraints(
                    "Valid values are: " + get_available_resolutions(s_color_resolution_map))));

        m_depth_resolution = parse_depth_resolution(declare_parameter(
            "depth.resolution",
            "1280x720",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The resolution of the depth camera.")
                .set__additional_constraints(
                    "Valid values are: " + get_available_resolutions(s_depth_resolution_map))));

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

        setup_device();
        m_output_queue->addCallback(std::bind(&Camera::callback, this, std::placeholders::_1));
        RCLCPP_INFO(this->get_logger(), "Started camera.");
    }

    void Camera::setup_device()
    {
        constexpr auto color_socket = dai::CameraBoardSocket::CAM_A;
        constexpr auto left_socket = dai::CameraBoardSocket::CAM_B;
        constexpr auto right_socket = dai::CameraBoardSocket::CAM_C;

        auto color = m_pipeline.create<dai::node::ColorCamera>();
        auto left = m_pipeline.create<dai::node::MonoCamera>();
        auto right = m_pipeline.create<dai::node::MonoCamera>();
        auto stereo = m_pipeline.create<dai::node::StereoDepth>();
        auto sync = m_pipeline.create<dai::node::Sync>();
        auto out = m_pipeline.create<dai::node::XLinkOut>();

        color->setBoardSocket(color_socket);
        color->setResolution(m_color_resolution);
        color->setFps(m_fps);
        color->setInterleaved(false);

        left->setBoardSocket(left_socket);
        left->setResolution(m_depth_resolution);
        left->setFps(m_fps);

        right->setBoardSocket(right_socket);
        right->setResolution(m_depth_resolution);
        right->setFps(m_fps);

        stereo->setDefaultProfilePreset(dai::node::StereoDepth::PresetMode::DEFAULT);
        stereo->setDepthAlign(color_socket);
        stereo->setLeftRightCheck(true);

        sync->setSyncThreshold(std::chrono::milliseconds(static_cast<int>((1.0f / m_fps) * 1000.0f * 0.5f)));

        out->setStreamName("out");

        // Link the left and right mono camera outputs to the stereo depth inputs.
        left->out.link(stereo->left);
        right->out.link(stereo->right);

        // Link the color and stereo depth outputs to the sync inputs so that we get synchronized frames.
        color->isp.link(sync->inputs["color"]);
        stereo->depth.link(sync->inputs["depth"]);

        // Link the sync output to the input of our output.
        sync->out.link(out->input);

        m_device = std::make_unique<dai::Device>(m_pipeline);
        m_output_queue = m_device->getOutputQueue("out", 8, false);

        // Get a single frame to retrieve calibration data.
        auto timed_out = false;
        auto message_group = m_output_queue->get<dai::MessageGroup>(std::chrono::seconds(5), timed_out);

        if (timed_out)
        {
            throw std::runtime_error("Timed out waiting for initial frames from camera.");
        }

        auto color_frame = message_group->get<dai::ImgFrame>("color");
        m_color_stream.m_image_converter = std::make_unique<dai::ros::ImageConverter>(m_frame_id, false, false);
        m_color_stream.m_camera_info = m_color_stream.m_image_converter->calibrationToCameraInfo(m_device->readCalibration(), color_socket, color_frame->getWidth(), color_frame->getHeight());

        auto depth_frame = message_group->get<dai::ImgFrame>("depth");
        m_depth_stream.m_image_converter = std::make_unique<dai::ros::ImageConverter>(m_frame_id, false, false);
        m_depth_stream.m_camera_info = m_depth_stream.m_image_converter->calibrationToCameraInfo(m_device->readCalibration(), color_socket, depth_frame->getWidth(), depth_frame->getHeight());
    }

    void Camera::callback(const std::shared_ptr<dai::ADatatype> &data)
    {
        auto message_group = std::dynamic_pointer_cast<dai::MessageGroup>(data);
        std::array<const char *, 2> frames = {"color", "depth"};
        std::array<CameraStream *, 2> streams = {&m_color_stream, &m_depth_stream};

        for (size_t index = 0; index < streams.size(); ++index)
        {
            auto frame = message_group->get<dai::ImgFrame>(frames[index]);
            const auto &stream = streams[index];

            sensor_msgs::msg::Image::SharedPtr image_msg = stream->m_image_converter->toRosMsgPtr(frame);
            auto camera_info_msg = std::make_shared<sensor_msgs::msg::CameraInfo>(stream->m_camera_info);
            camera_info_msg->header = image_msg->header;
            stream->m_publisher.publish(image_msg, camera_info_msg);
        }
    }

    dai::ColorCameraProperties::SensorResolution Camera::parse_color_resolution(const std::string &resolution)
    {
        auto iterator = s_color_resolution_map.find(resolution);

        if (iterator != s_color_resolution_map.end())
        {
            return iterator->second;
        }
        throw std::runtime_error("Invalid color resolution");
    }

    dai::MonoCameraProperties::SensorResolution Camera::parse_depth_resolution(const std::string &resolution)
    {
        auto iterator = s_depth_resolution_map.find(resolution);

        if (iterator != s_depth_resolution_map.end())
        {
            return iterator->second;
        }
        throw std::runtime_error("Invalid depth resolution");
    }
}