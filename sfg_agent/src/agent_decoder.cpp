#include "sfg_agent/agent_decoder.hpp"

#include "isaac_ros_h264_decoder/decoder_node.hpp"
#include "sfg_agent/agent_heartbeat_constants.hpp"
#include "sfg_utils/sanitize_hostname.hpp"

#define STRINGIFY(value) #value

namespace sfg_agent
{
    AgentDecoder::AgentDecoder(const rclcpp::NodeOptions &options) : Node("agent_decoder", options)
    {
        // Declare and retrieve ROS parameters.
        std::string parameter = "container_name";
        declare_parameter<std::string>(
            parameter,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The name of the container decoder nodes should be dymically loaded in."));
        get_parameter(parameter, m_container_name);

        parameter = "hostname";
        declare_parameter<std::string>(
            parameter,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The agent hostname that should be decoded if present."));
        get_parameter(parameter, m_hostname);
        // We need to use a sanitized version of the hostname for ROS communication.
        m_sanitized_hostname = sfg_utils::sanitize_hostname(m_hostname);

        parameter = "keepalive";
        declare_parameter(
            parameter,
            3,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The number of heartbeat messages to wait before considering an agent dead."));
        get_parameter(parameter, m_keepalive);
        m_keepalive_count = m_keepalive;

        // Set up interfaces.
        m_agent_heartbeat_subscriber = create_subscription<sfg_agent_msgs::msg::AgentHeartbeat>(
            "/global/agent_heartbeat", rclcpp::QoS(rclcpp::KeepLast(1)).reliable(),
            std::bind(&AgentDecoder::heartbeat_callback, this, std::placeholders::_1));
        m_get_agent_metadata_client =
            create_client<sfg_agent_msgs::srv::GetAgentMetadata>("/global/" + m_sanitized_hostname + "/get_agent_metadata");

        std::string load_node_service = m_container_name + "/_container/load_node";
        std::string unload_node_service = m_container_name + "/_container/unload_node";
        RCLCPP_INFO(get_logger(), "Creating client for '%s' and '%s'.", load_node_service.c_str(), unload_node_service.c_str());
        m_load_node_client = create_client<composition_interfaces::srv::LoadNode>(load_node_service);
        m_unload_node_client = create_client<composition_interfaces::srv::UnloadNode>(unload_node_service);

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

        if (!m_get_agent_metadata_client->service_is_ready())
        {
            RCLCPP_WARN(get_logger(), "Get agent metadata service client for '%s' not ready.", m_hostname.c_str());
            return;
        }

        RCLCPP_INFO(get_logger(), "Requesting agent metadata for '%s'.", m_hostname.c_str());

        auto weak_this = weak_from_this();

        m_get_agent_metadata_client->async_send_request(
            std::make_shared<sfg_agent_msgs::srv::GetAgentMetadata::Request>(),
            [weak_this](rclcpp::Client<sfg_agent_msgs::srv::GetAgentMetadata>::SharedFuture future)
            {
                if (weak_this.expired())
                {
                    RCLCPP_WARN(
                        rclcpp::get_logger(STRINGIFY(AgentDecoder::heartbeat_callback)),
                        STRINGIFY(AgentDecoder) " object expired before metadata callback.");
                    return;
                }

                auto agent_decoder = std::static_pointer_cast<AgentDecoder>(weak_this.lock());
                agent_decoder->agent_metadata_callback(std::move(future));
            });
    }

    void AgentDecoder::agent_metadata_callback(rclcpp::Client<sfg_agent_msgs::srv::GetAgentMetadata>::SharedFuture future)
    {
        if (!future.valid())
        {
            RCLCPP_ERROR(get_logger(), "Failed to get agent metadata.");
            return;
        }

        auto response = future.get();

        for (const auto &camera : response->cameras)
        {
            RCLCPP_INFO(get_logger(), "Adding camera decoder node for '%s' for agent '%s'", camera.c_str(), m_hostname.c_str());

            std::string input_topic = "/global/" + m_sanitized_hostname + "/" + camera + "/color_compressed";
            std::string output_topic = "/local/" + m_sanitized_hostname + "/" + camera + "/color_uncompressed";

            load_node(
                "isaac_ros_h264_decoder",
                "nvidia::isaac_ros::h264_decoder::DecoderNode",
                camera + "_decoder",
                {"image_compressed" + (":=" + input_topic),
                 "image_uncompressed" + (":=" + output_topic)});
        }

        for (const auto &lidar : response->lidars)
        {
            RCLCPP_INFO(get_logger(), "Adding lidar decoder node for '%s' for agent '%s'", lidar.c_str(), m_hostname.c_str());

            std::string input_topic = "/global/" + m_sanitized_hostname + "/" + lidar + "/pcl_compressed";
            std::string output_topic = "/local/" + m_sanitized_hostname + "/" + lidar + "/pcl_uncompressed";
        }

        // After retrieving the metadata, we can start the keepalive timer.
        m_keepalive_timer = create_wall_timer(
            std::chrono::seconds(AGENT_HEARTBEAT_INTERVAL),
            std::bind(&AgentDecoder::keepalive_callback, this));
    }

    void AgentDecoder::keepalive_callback()
    {
        if (m_keepalive_count != 0)
        {
            m_keepalive_count--;
            return;
        }

        RCLCPP_WARN(get_logger(), "Agent '%s' died.", m_hostname.c_str());

        for (const auto &id : m_loaded_node_ids)
        {
            unload_node(id);
        }
        m_loaded_node_ids.clear();
        m_keepalive_timer = nullptr;
    }

    void AgentDecoder::load_node(
        const std::string &package_name,
        const std::string &plugin_name,
        const std::string &node_name,
        const std::vector<std::string> &remapping_rules)
    {
        if (!m_load_node_client->service_is_ready())
        {
            RCLCPP_ERROR(
                get_logger(),
                "Load node service client not ready. Cannot load node '%s' in package '%s'.",
                plugin_name.c_str(),
                package_name.c_str());
            return;
        }

        auto request = std::make_shared<composition_interfaces::srv::LoadNode::Request>();
        request->package_name = package_name;
        request->plugin_name = plugin_name;
        request->node_name = node_name;
        request->node_namespace = "/local/" + m_sanitized_hostname;
        request->remap_rules = remapping_rules;

        auto weak_this = weak_from_this();

        m_load_node_client->async_send_request(
            request,
            [weak_this, package_name, plugin_name](rclcpp::Client<composition_interfaces::srv::LoadNode>::SharedFuture future)
            {
                if (weak_this.expired())
                {
                    RCLCPP_WARN(
                        rclcpp::get_logger(STRINGIFY(AgentDecoder::load_node)),
                        STRINGIFY(AgentDecoder) " object expired before load node callback for node '%s' from package '%s'.",
                        package_name.c_str(),
                        plugin_name.c_str());
                    return;
                }

                auto agent_decoder = std::static_pointer_cast<AgentDecoder>(weak_this.lock());

                if (!future.valid())
                {
                    RCLCPP_ERROR(
                        agent_decoder->get_logger(),
                        "Failed to load node '%s' from package '%s'.",
                        plugin_name.c_str(),
                        package_name.c_str());
                    return;
                }

                auto response = future.get();

                if (!response->success)
                {
                    RCLCPP_ERROR(
                        agent_decoder->get_logger(),
                        "Failed to load node '%s' from package '%s': %s",
                        plugin_name.c_str(),
                        package_name.c_str(),
                        response->error_message.c_str());
                    return;
                }

                agent_decoder->m_loaded_node_ids.push_back(response->unique_id);
                RCLCPP_INFO(agent_decoder->get_logger(), "Loaded node '%s' from package '%s'.", plugin_name.c_str(), package_name.c_str());
            });
    }

    void AgentDecoder::unload_node(const uint64_t id)
    {
        if (!m_unload_node_client->service_is_ready())
        {
            RCLCPP_ERROR(get_logger(), "Unload node service client not ready. Cannot unload node with ID %lu.", id);
            return;
        }

        auto request = std::make_shared<composition_interfaces::srv::UnloadNode::Request>();
        request->unique_id = id;

        auto weak_this = weak_from_this();

        m_unload_node_client->async_send_request(
            request,
            [weak_this, id](rclcpp::Client<composition_interfaces::srv::UnloadNode>::SharedFuture future)
            {
                if (weak_this.expired())
                {
                    RCLCPP_WARN(
                        rclcpp::get_logger(STRINGIFY(AgentDecoder::unload_node)),
                        STRINGIFY(AgentDecoder) " object expired before unload node callback for ID '%lu'.",
                        id);
                    return;
                }

                auto agent_decoder = std::static_pointer_cast<AgentDecoder>(weak_this.lock());

                if (!future.valid())
                {
                    RCLCPP_ERROR(agent_decoder->get_logger(), "Failed to unload node with ID '%lu'.", id);
                    return;
                }

                auto response = future.get();

                if (!response->success)
                {
                    RCLCPP_ERROR(
                        agent_decoder->get_logger(),
                        "Failed to unload node with ID '%lu': %s", id,
                        response->error_message.c_str());
                    return;
                }
                RCLCPP_INFO(agent_decoder->get_logger(), "Unloaded node with ID '%lu'.", id);
            });
    }
}