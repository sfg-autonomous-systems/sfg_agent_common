#include "sfg_agent/agent_decoder.hpp"

#include <isaac_ros_h264_decoder/decoder_node.hpp>
#include <regex>

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

        parameter = "hostname_regex";
        declare_parameter<std::string>(
            parameter,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The regex to match hostnames against."
                                  "If the hostname matches, the agent will be decoded."));
        get_parameter(parameter, m_hostname_regex);
        m_compiled_hostname_regex = std::regex(m_hostname_regex);

        parameter = "keepalive";
        declare_parameter(
            parameter,
            3,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The number of heartbeat messages to wait before considering an agent dead."));
        get_parameter(parameter, m_keepalive);

        m_callback_group = create_callback_group(rclcpp::CallbackGroupType::Reentrant);

        // Set up interfaces.
        rclcpp::SubscriptionOptions subscription_options;
        subscription_options.callback_group = m_callback_group;
        m_agent_heartbeat_subscriber = create_subscription<sfg_agent_msgs::msg::AgentHeartbeat>(
            "/global/agent_heartbeat", rclcpp::QoS(rclcpp::KeepLast(1)).reliable(),
            std::bind(&AgentDecoder::heartbeat_callback, this, std::placeholders::_1),
            subscription_options);

        std::string load_node_service = m_container_name + "/_container/load_node";
        std::string unload_node_service = m_container_name + "/_container/unload_node";
        RCLCPP_INFO(get_logger(), "Creating client for '%s' and '%s'.", load_node_service.c_str(), unload_node_service.c_str());
        m_load_node_client = create_client<composition_interfaces::srv::LoadNode>(
            load_node_service,
            rmw_qos_profile_services_default,
            m_callback_group);
        m_unload_node_client = create_client<composition_interfaces::srv::UnloadNode>(
            unload_node_service,
            rmw_qos_profile_services_default,
            m_callback_group);

        RCLCPP_INFO(get_logger(), "Started agent decoder for agents matching regex '%s'.", m_hostname_regex.c_str());
    }

    void AgentDecoder::heartbeat_callback(const sfg_agent_msgs::msg::AgentHeartbeat::SharedPtr msg)
    {
        auto hostname = msg->hostname;

        // Ignore any hostname that doesn't match the regex.
        if (!std::regex_match(hostname, m_compiled_hostname_regex))
        {
            return;
        }

        std::shared_ptr<AgentData> agent_data;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto iterator = m_decoded_agents.find(hostname);

            if (iterator != m_decoded_agents.end())
            {
                iterator->second->m_keepalive_timer->reset();
                return;
            }

            m_decoded_agents[hostname] = agent_data = std::make_shared<AgentData>();
        }

        auto weak_this = weak_from_this();
        auto weak_agent_data = std::weak_ptr<AgentData>(agent_data);

        RCLCPP_INFO(get_logger(), "Agent '%s' is alive.", hostname.c_str());

        agent_data->m_hostname = hostname;
        agent_data->m_sanitized_hostname = sfg_utils::sanitize_hostname(hostname);
        agent_data->m_keepalive_timer = create_wall_timer(
            std::chrono::seconds(m_keepalive * AGENT_HEARTBEAT_INTERVAL),
            [weak_this, weak_agent_data]()
            {
                if (auto shared_this = weak_this.lock())
                {
                    auto agent_decoder = std::dynamic_pointer_cast<AgentDecoder>(shared_this);
                    agent_decoder->keepalive_callback(weak_agent_data);
                }
            });

        agent_data->m_get_metadata_client = create_client<sfg_agent_msgs::srv::GetAgentMetadata>(
            "/global/" + sfg_utils::sanitize_hostname(hostname) + "/get_agent_metadata",
            rmw_qos_profile_services_default,
            m_callback_group);

        if (!agent_data->m_get_metadata_client->service_is_ready())
        {
            RCLCPP_WARN(get_logger(), "Get agent metadata service for '%s' not ready.", hostname.c_str());
            return;
        }

        RCLCPP_INFO(get_logger(), "Requesting agent metadata for '%s'.", hostname.c_str());

        agent_data->m_get_metadata_client->async_send_request(
            std::make_shared<sfg_agent_msgs::srv::GetAgentMetadata::Request>(),
            [weak_this, weak_agent_data](rclcpp::Client<sfg_agent_msgs::srv::GetAgentMetadata>::SharedFuture future)
            {
                if (auto shared_this = weak_this.lock())
                {
                    auto agent_decoder = std::dynamic_pointer_cast<AgentDecoder>(shared_this);
                    agent_decoder->agent_metadata_callback(weak_agent_data, future);
                }
            });
    }

    void AgentDecoder::keepalive_callback(const std::weak_ptr<AgentData> weak_agent_data)
    {
        std::shared_ptr<AgentData> agent_data = weak_agent_data.lock();

        if (!agent_data)
        {
            RCLCPP_WARN(get_logger(), "Agent '%s' is dead.", agent_data->m_hostname.c_str());
        }

        RCLCPP_INFO(get_logger(), "Agent '%s' died.", agent_data->m_hostname.c_str());

        for (const auto &node : agent_data->m_loaded_node_ids)
        {
            unload_node(node);
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_decoded_agents.erase(agent_data->m_hostname);
    }

    void AgentDecoder::agent_metadata_callback(
        const std::weak_ptr<AgentData> weak_agent_data,
        rclcpp::Client<sfg_agent_msgs::srv::GetAgentMetadata>::SharedFuture future)
    {
        auto agent_data = weak_agent_data.lock();

        if (!agent_data)
        {
            RCLCPP_WARN(get_logger(), "Agent has died since service request to get metadata.");
            return;
        }

        if (!future.valid())
        {
            RCLCPP_ERROR(get_logger(), "Failed to get agent metadata.");
            return;
        }

        auto response = future.get();

        for (const auto &camera : response->cameras)
        {
            RCLCPP_INFO(get_logger(), "Adding camera decoder node for '%s' for agent '%s'", camera.c_str(), agent_data->m_hostname.c_str());

            std::string input_topic = "/global/" + agent_data->m_sanitized_hostname + "/" + camera + "/color_compressed";
            std::string output_topic = "/local/" + agent_data->m_sanitized_hostname + "/" + camera + "/color_uncompressed";

            load_node(
                agent_data,
                "isaac_ros_h264_decoder",
                "nvidia::isaac_ros::h264_decoder::DecoderNode",
                camera + "_decoder",
                {"image_compressed" + (":=" + input_topic),
                 "image_uncompressed" + (":=" + output_topic)});
        }

        for (const auto &lidar : response->lidars)
        {
            RCLCPP_INFO(get_logger(), "Adding lidar decoder node for '%s' for agent '%s'", lidar.c_str(), agent_data->m_hostname.c_str());

            std::string input_topic = "/global/" + agent_data->m_sanitized_hostname + "/" + lidar + "/pcl_compressed";
            std::string output_topic = "/local/" + agent_data->m_sanitized_hostname + "/" + lidar + "/pcl_uncompressed";
        }
    }

    void AgentDecoder::load_node_callback(
        const std::weak_ptr<AgentData> weak_agent_data,
        const std::string &package_name,
        const std::string &plugin_name,
        rclcpp::Client<composition_interfaces::srv::LoadNode>::SharedFuture future)
    {
        if (!future.valid())
        {
            RCLCPP_ERROR(
                get_logger(),
                "Failed to load node '%s' from package '%s'.",
                plugin_name.c_str(),
                package_name.c_str());
            return;
        }

        auto response = future.get();

        if (!response->success)
        {
            RCLCPP_ERROR(
                get_logger(),
                "Failed to load node '%s' from package '%s': %s",
                plugin_name.c_str(),
                package_name.c_str(),
                response->error_message.c_str());
            return;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto agent_data = weak_agent_data.lock();

            if (!agent_data)
            {
                RCLCPP_WARN(get_logger(), "Agent has died since service request to load node. Unloading node '%s' from package '%s' again.", plugin_name.c_str(), package_name.c_str());
                unload_node(response->unique_id);
                return;
            }
            agent_data->m_loaded_node_ids.push_back(response->unique_id);
        }

        RCLCPP_INFO(get_logger(), "Loaded node '%s' from package '%s'.", plugin_name.c_str(), package_name.c_str());
    }

    void AgentDecoder::unload_node_callback(uint64_t id, rclcpp::Client<composition_interfaces::srv::UnloadNode>::SharedFuture future)
    {
        if (!future.valid())
        {
            RCLCPP_ERROR(get_logger(), "Failed to unload node with ID '%lu'.", id);
            return;
        }

        auto response = future.get();

        if (!response->success)
        {
            RCLCPP_ERROR(
                get_logger(),
                "Failed to unload node with ID '%lu': %s", id,
                response->error_message.c_str());
            return;
        }

        RCLCPP_INFO(get_logger(), "Unloaded node with ID '%lu'.", id);
    }

    void AgentDecoder::load_node(
        const std::shared_ptr<AgentData> agent_data,
        const std::string &package_name,
        const std::string &plugin_name,
        const std::string &node_name,
        const std::vector<std::string> &remapping_rules)
    {
        if (!m_load_node_client->service_is_ready())
        {
            RCLCPP_ERROR(
                get_logger(),
                "Load node service not ready. Cannot load node '%s' in package '%s'.",
                plugin_name.c_str(),
                package_name.c_str());
            return;
        }

        auto request = std::make_shared<composition_interfaces::srv::LoadNode::Request>();
        request->package_name = package_name;
        request->plugin_name = plugin_name;
        request->node_name = node_name;
        request->node_namespace = "/local/" + agent_data->m_sanitized_hostname;
        request->remap_rules = remapping_rules;

        auto weak_this = weak_from_this();
        auto weak_agent_data = std::weak_ptr<AgentData>(agent_data);

        m_load_node_client->async_send_request(
            request,
            [weak_this, weak_agent_data, package_name, plugin_name](rclcpp::Client<composition_interfaces::srv::LoadNode>::SharedFuture future)
            {
                if (auto shared_this = weak_this.lock())
                {
                    auto agent_decoder = std::dynamic_pointer_cast<AgentDecoder>(shared_this);
                    agent_decoder->load_node_callback(weak_agent_data, package_name, plugin_name, future);
                }
            });
    }

    void AgentDecoder::unload_node(uint64_t id)
    {
        if (!m_unload_node_client->service_is_ready())
        {
            RCLCPP_ERROR(get_logger(), "Unload node service not ready. Cannot unload node with ID '%lu'.", id);
            return;
        }

        auto request = std::make_shared<composition_interfaces::srv::UnloadNode::Request>();
        request->unique_id = id;

        auto weak_this = weak_from_this();

        m_unload_node_client->async_send_request(
            request,
            [weak_this, id](rclcpp::Client<composition_interfaces::srv::UnloadNode>::SharedFuture future)
            {
                if (auto shared_this = weak_this.lock())
                {
                    auto agent_decoder = std::dynamic_pointer_cast<AgentDecoder>(shared_this);
                    agent_decoder->unload_node_callback(id, future);
                }
            });
    }
}