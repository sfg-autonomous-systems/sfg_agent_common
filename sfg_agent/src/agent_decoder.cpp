#include "sfg_agent/agent_decoder.hpp"

#include "isaac_ros_h264_decoder/decoder_node.hpp"
#include "sfg_agent/agent_heartbeat_constants.hpp"
#include "sfg_utils/sanitize_hostname.hpp"

namespace sfg_agent
{
    AgentDecoder::AgentDecoder(rclcpp::Executor &executor) : Node("agent_decoder"), m_executor(executor)
    {

        // Declare and retrieve ROS parameters.
        declare_parameter<std::string>(
            "hostname",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The agent hostname that should be decoded if present."));
        get_parameter("hostname", m_hostname);
        m_hostname = sfg_utils::sanitize_hostname(m_hostname);
        declare_parameter(
            "keepalive",
            3,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The number of heartbeat messages to wait before considering an agent dead."));
        get_parameter("keepalive_timeout", m_keepalive);
        m_keepalive_count = m_keepalive;

        // Set up interfaces.
        m_heartbeat_subscriber = create_subscription<sfg_agent_msgs::msg::AgentHeartbeat>(
            "/global/agent_heartbeat", rclcpp::QoS(rclcpp::KeepLast(1)).reliable(),
            std::bind(&AgentDecoder::heartbeat_callback, this, std::placeholders::_1));
        m_agent_metadata_client = create_client<sfg_agent_msgs::srv::GetAgentMetadata>(
            "/global/" + m_hostname + "/get_agent_metadata");

        RCLCPP_INFO(get_logger(), "Started agent decoder for '%s'.", m_hostname.c_str());
    }

    void AgentDecoder::heartbeat_callback(const sfg_agent_msgs::msg::AgentHeartbeat::SharedPtr msg)
    {
        if (msg->hostname != m_hostname)
        {
            return;
        }

        m_keepalive_count = m_keepalive;

        if (m_keepalive_timer)
        {
            return;
        }

        RCLCPP_INFO(get_logger(), "Agent '%s' is alive.", m_hostname.c_str());

        // This was the first time we received a heartbeat from this agent.
        m_keepalive_timer = create_wall_timer(
            std::chrono::seconds(AGENT_HEARTBEAT_INTERVAL),
            std::bind(&AgentDecoder::keepalive_callback, this));
        m_agent_metadata_client->async_send_request(
            std::make_shared<sfg_agent_msgs::srv::GetAgentMetadata::Request>(),
            std::bind(&AgentDecoder::agent_metadata_callback, this, std::placeholders::_1));
    }

    void AgentDecoder::agent_metadata_callback(
        rclcpp::Client<sfg_agent_msgs::srv::GetAgentMetadata>::SharedFuture future)
    {
        if (!future.valid())
        {
            RCLCPP_ERROR(get_logger(), "Failed to get agent metadata.");
            return;
        }

        RCLCPP_INFO(get_logger(), "Adding required decoder nodes for agent '%s'", m_hostname.c_str());
        auto agent_metadata = future.get();

        if ((agent_metadata->capabilities & sfg_agent_msgs::srv::GetAgentMetadata::Response::CAPABILITY_CAMERA) != 0)
        {
            for (const auto &camera : agent_metadata->cameras)
            {
                std::string input_topic = "/global/" + m_hostname + "/" + camera + "/color_compressed";
                std::string output_topic = "/local/" + m_hostname + "/" + camera + "/color_raw";

                RCLCPP_INFO(
                    get_logger(),
                    "Adding decoder node with input topic '%s' and output topic '%s' for camera '%s'",
                    input_topic.c_str(),
                    output_topic.c_str(),
                    m_hostname.c_str());

                rclcpp::NodeOptions options;
                options.arguments({"-r", input_topic, "-r", output_topic});
                auto node = std::make_shared<nvidia::isaac_ros::h264_decoder::DecoderNode>(options);
                m_executor.add_node(node);
                m_decoder_nodes.push_back(node);
            }
        }

        if ((agent_metadata->capabilities & sfg_agent_msgs::srv::GetAgentMetadata::Response::CAPABILITY_LIDAR) != 0)
        {
            for (const auto &lidar : agent_metadata->lidars)
            {
                std::string input_topic = "/global/" + m_hostname + "/" + lidar + "/lidar_compressed";
                std::string output_topic = "/local/" + m_hostname + "/" + lidar + "/lidar_raw";

                RCLCPP_INFO(
                    get_logger(),
                    "Adding decoder node with input topic '%s' and output topic '%s' for lidar '%s'",
                    input_topic.c_str(),
                    output_topic.c_str(),
                    m_hostname.c_str());

                rclcpp::NodeOptions options;
                options.arguments({"-r", input_topic, "-r", output_topic});
                // ToDo: Add lidar decoder node.
            }
        }
    }

    void AgentDecoder::keepalive_callback()
    {
        if (m_keepalive_count != 0)
        {
            m_keepalive_count--;
            return;
        }

        RCLCPP_WARN(get_logger(), "Agent '%s' died.", m_hostname.c_str());

        for (const auto &node : m_decoder_nodes)
        {
            m_executor.remove_node(node);
        }
        m_decoder_nodes.clear();
        m_keepalive_timer = nullptr;
    }
}