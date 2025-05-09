#include "sfg_agent/agent_decoder.hpp"

#include "sfg_agent/agent_heartbeat_constants.hpp"
#include "sfg_utils/extract_parameters.hpp"
#include "sfg_utils/sanitize_agent_name.hpp"

namespace sfg_agent
{
    AgentDecoder::AgentDecoder(const rclcpp::NodeOptions &options) : Node("agent_decoder", options),
                                                                     m_parameters(extract_parameters(options))
    {
        // Declare and retrieve ROS parameters.
        std::string parameter = "container_name";
        declare_parameter<std::string>(
            parameter,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The name of the container decoder nodes should be dymically loaded in."));
        get_parameter(parameter, m_container_name);

        parameter = "agent_name_regex";
        declare_parameter(
            parameter,
            ".*",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The regex to match agent names against."
                                  "If the agent name matches, the agent will be decoded."));
        get_parameter(parameter, m_agent_name_regex);
        m_compiled_agent_name_regex = std::regex(m_agent_name_regex);

        // Set up interfaces.
        m_agent_discovery_event_subscriber = create_subscription<sfg_agent_msgs::msg::AgentDiscoveryEvent>(
            "agent_discovery_event",
            rclcpp::QoS(rclcpp::KeepAll()).reliable(),
            [this](const sfg_agent_msgs::msg::AgentDiscoveryEvent::SharedPtr msg)
            {
                handle_agent_disovery_event(msg->metadata, msg->event_type);
            });

        m_get_discovered_agents_client = create_client<sfg_agent_msgs::srv::GetDiscoveredAgents>("get_discovered_agents");

        std::string load_node_service = m_container_name + "/_container/load_node";
        RCLCPP_INFO(get_logger(), "Creating client for service '%s'.", load_node_service.c_str());
        m_load_node_client = create_client<composition_interfaces::srv::LoadNode>(
            load_node_service);

        std::string unload_node_service = m_container_name + "/_container/unload_node";
        RCLCPP_INFO(get_logger(), "Creating client for service '%s'.", unload_node_service.c_str());
        m_unload_node_client = create_client<composition_interfaces::srv::UnloadNode>(
            unload_node_service);

        RCLCPP_INFO(get_logger(), "Started agent decoder for agents matching regex '%s'.", m_agent_name_regex.c_str());

        auto request = std::make_shared<sfg_agent_msgs::srv::GetDiscoveredAgents::Request>();
        m_get_discovered_agents_client->async_send_request(
            request,
            std::bind(&AgentDecoder::get_discovered_agents_callback, this, std::placeholders::_1));
    }

    void AgentDecoder::handle_agent_disovery_event(const sfg_agent_msgs::msg::AgentMetadata &metadata, uint8_t event_type)
    {
        auto agent_name = metadata.agent_name;

        if (!std::regex_match(agent_name, m_compiled_agent_name_regex))
        {
            return;
        }

        auto iterator = m_decoded_agents.find(agent_name);

        switch (event_type)
        {
        case sfg_agent_msgs::msg::AgentDiscoveryEvent::DISCOVERED:
        {
            if (iterator != m_decoded_agents.end())
            {
                RCLCPP_WARN(get_logger(), "Skipping loading nodes for agent '%s': Agent already exists in decoded agents.", agent_name.c_str());
                return;
            }
            auto agent = m_decoded_agents[agent_name] = std::make_shared<DecodedAgent>();
            load_nodes(agent, metadata);
            break;
        }
        case sfg_agent_msgs::msg::AgentDiscoveryEvent::LOST:
        {
            if (iterator == m_decoded_agents.end())
            {
                RCLCPP_WARN(get_logger(), "Cannot unload nodes for agent '%s': Agent not found in decoded agents.", agent_name.c_str());
                return;
            }
            unload_nodes(iterator->second);
            m_decoded_agents.erase(iterator);
            break;
        }
        }
    }

    void AgentDecoder::get_discovered_agents_callback(rclcpp::Client<sfg_agent_msgs::srv::GetDiscoveredAgents>::SharedFuture future)
    {
        if (!future.valid())
        {
            RCLCPP_ERROR(get_logger(), "Failed to get discovered agents: Future is invalid.");
            return;
        }

        std::shared_ptr<sfg_agent_msgs::srv::GetDiscoveredAgents::Response> response;

        try
        {
            response = future.get();
        }
        catch (const std::exception &exception)
        {
            RCLCPP_ERROR(get_logger(), "Failed to get discovered agents: %s", exception.what());
            return;
        }

        for (const auto &metadata : response->metadata)
        {
            handle_agent_disovery_event(metadata, sfg_agent_msgs::msg::AgentDiscoveryEvent::DISCOVERED);
        }
    }

    void AgentDecoder::load_nodes(
        std::shared_ptr<DecodedAgent> agent,
        const sfg_agent_msgs::msg::AgentMetadata &metadata)
    {
        auto sanitized_hostname = sfg_utils::sanitize_agent_name(metadata.agent_name);
        auto weak_agent = std::weak_ptr<DecodedAgent>(agent);

        for (const auto &camera : metadata.cameras)
        {
            RCLCPP_INFO(get_logger(), "Adding camera color decoder node for '%s' for agent '%s'.", camera.c_str(), metadata.agent_name.c_str());

            std::string package_name = "sfg_image_transport";
            std::string plugin_name = "sfg_image_transport::Republisher";
            std::string input_topic = "/global/" + sanitized_hostname + "/" + camera + "/color_compressed";
            std::string output_topic = get_namespace() + ("/" + sanitized_hostname) + "/" + camera + "/color";

            auto request = std::make_shared<composition_interfaces::srv::LoadNode::Request>();
            request->package_name = package_name;
            request->plugin_name = plugin_name;
            request->node_name = camera + "_color_decoder";
            request->node_namespace = get_namespace() + ("/" + sanitized_hostname);
            request->parameters = {
                rclcpp::Parameter("in_transport", "ffmpeg").to_parameter_msg(),
                rclcpp::Parameter("out.enable_pub_plugins", std::vector<std::string>({"image_transport/raw"})).to_parameter_msg()};
            request->parameters.insert(request->parameters.end(), m_parameters.begin(), m_parameters.end());
            request->extra_arguments = {rclcpp::Parameter("use_intra_process_comms", get_node_options().use_intra_process_comms()).to_parameter_msg()};
            request->remap_rules = {"in/ffmpeg" + (":=" + input_topic), "out" + (":=" + output_topic)};

            m_load_node_client->async_send_request(
                request, [this, weak_agent, package_name, plugin_name](rclcpp::Client<composition_interfaces::srv::LoadNode>::SharedFuture future)
                { load_node_callback(weak_agent, package_name, plugin_name, future); });

            RCLCPP_INFO(get_logger(), "Adding camera depth decoder node for '%s' for agent '%s'.", camera.c_str(), metadata.agent_name.c_str());

            package_name = "sfg_image_transport";
            plugin_name = "sfg_image_transport::Republisher";
            input_topic = "/global/" + sanitized_hostname + "/" + camera + "/depth_compressed";
            output_topic = get_namespace() + ("/" + sanitized_hostname) + "/" + camera + "/depth";

            request = std::make_shared<composition_interfaces::srv::LoadNode::Request>();
            request->package_name = package_name;
            request->plugin_name = plugin_name;
            request->node_name = camera + "_depth_decoder";
            request->node_namespace = get_namespace() + ("/" + sanitized_hostname);
            request->parameters = {
                rclcpp::Parameter("in_transport", "compressedDepth").to_parameter_msg(),
                rclcpp::Parameter("out.enable_pub_plugins", std::vector<std::string>({"image_transport/raw"})).to_parameter_msg()};
            request->parameters.insert(request->parameters.end(), m_parameters.begin(), m_parameters.end());
            request->extra_arguments = {rclcpp::Parameter("use_intra_process_comms", get_node_options().use_intra_process_comms()).to_parameter_msg()};
            request->remap_rules = {"in/compressedDepth" + (":=" + input_topic), "out" + (":=" + output_topic)};

            m_load_node_client->async_send_request(
                request, [this, weak_agent, package_name, plugin_name](rclcpp::Client<composition_interfaces::srv::LoadNode>::SharedFuture future)
                { load_node_callback(weak_agent, package_name, plugin_name, future); });
        }
    }

    void AgentDecoder::load_node_callback(
        std::weak_ptr<DecodedAgent> weak_agent,
        const std::string &package_name,
        const std::string &plugin_name,
        rclcpp::Client<composition_interfaces::srv::LoadNode>::SharedFuture future)
    {
        // ToDo: Perhaps we should think of implementing some sort of retry logic if loading of the node failed.

        if (!future.valid())
        {
            RCLCPP_ERROR(get_logger(), "Failed to load node '%s' from package '%s': Future is invalid", plugin_name.c_str(), package_name.c_str());
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

        auto id = response->unique_id;

        if (auto agent = weak_agent.lock())
        {
            agent->m_loaded_decoders.push_back(std::make_tuple(package_name, plugin_name, id));
            RCLCPP_INFO(get_logger(), "Loaded node '%s' from package '%s'.", plugin_name.c_str(), package_name.c_str());
        }
        else
        {
            // The agent has been lost since the service to load the node was called.
            // Therefore, the node is no longer needed and we should unload it.
            auto unload_request = std::make_shared<composition_interfaces::srv::UnloadNode::Request>();
            unload_request->unique_id = id;

            m_unload_node_client->async_send_request(
                unload_request,
                [this, package_name, plugin_name, id](rclcpp::Client<composition_interfaces::srv::UnloadNode>::SharedFuture future)
                {
                    unload_node_callback(package_name, plugin_name, id, future);
                });

            RCLCPP_WARN(get_logger(), "Loaded node '%s' from package '%s' but associated agent was lost in the meantime. Unloading node again.", plugin_name.c_str(), package_name.c_str());
        }
    }

    void AgentDecoder::unload_nodes(std::shared_ptr<DecodedAgent> agent)
    {
        for (const auto &[package_name, plugin_name, id] : agent->m_loaded_decoders)
        {
            auto request = std::make_shared<composition_interfaces::srv::UnloadNode::Request>();
            request->unique_id = id;

            m_unload_node_client->async_send_request(
                request,
                [this, package_name, plugin_name, id](rclcpp::Client<composition_interfaces::srv::UnloadNode>::SharedFuture future)
                {
                    unload_node_callback(package_name, plugin_name, id, future);
                });
        }
    }

    void AgentDecoder::unload_node_callback(
        const std::string &package_name,
        const std::string &plugin_name,
        uint64_t id,
        rclcpp::Client<composition_interfaces::srv::UnloadNode>::SharedFuture future)
    {
        if (!future.valid())
        {
            RCLCPP_ERROR(get_logger(), "Failed to unload node '%s' from package '%s' with ID '%lu': Future is invalid.", package_name.c_str(), plugin_name.c_str(), id);
            return;
        }

        auto response = future.get();

        if (!response->success)
        {
            RCLCPP_ERROR(get_logger(), "Failed to unload node '%s' from package '%s' with ID '%lu': %s", package_name.c_str(), plugin_name.c_str(), id, response->error_message.c_str());
            return;
        }

        RCLCPP_INFO(get_logger(), "Unloaded node with ID '%lu'.", id);
    }
}