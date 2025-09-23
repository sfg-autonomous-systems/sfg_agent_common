#include "sfg_agent/agent_decoder.hpp"

#include <magic_enum.hpp>

#include "sfg_agent/constants.hpp"
#include "sfg_utils/agent_utils.hpp"
#include "sfg_utils/ros_utils.hpp"
#include "sfg_utils/fqn/ros_fqn_builder.hpp"

namespace sfg_agent
{
    AgentDecoder::AgentDecoder(const rclcpp::NodeOptions &options)
        : Node("agent_decoder", rclcpp::NodeOptions(options).allow_undeclared_parameters(true).automatically_declare_parameters_from_overrides(true))
    {
        // Declare and retrieve ROS parameters.
        m_container_name = sfg_utils::ros_utils::declare_parameter_if_not_declared<std::string>(
            *this,
            "container_name",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The name of the container decoder nodes should be dymically loaded in."));

        m_agent_name_regex = sfg_utils::ros_utils::declare_parameter_if_not_declared(
            *this,
            "agent_name_regex",
            ".*",
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The regex to match agent names against."
                                  "If the agent name matches, the agent will be decoded."));
        m_compiled_agent_name_regex = std::regex(m_agent_name_regex);

        m_camera_decoder_parameters = sfg_utils::ros_utils::extract_parameters<rcl_interfaces::msg::Parameter>(*this, "camera_decoder_parameters");
        m_camera_info_relay_parameters = sfg_utils::ros_utils::extract_parameters<rcl_interfaces::msg::Parameter>(*this, "camera_info_relay_parameters");

        using namespace sfg_utils::fqn;

        // Set up interfaces.
        m_agent_discovery_event_subscriber = create_subscription<sfg_agent_msgs::msg::DiscoveryEvent>(
            RosFqnBuilder().scope(Scope::Local).agent().resource(Resource::Custom, "agent_discovery_event").build(),
            10,
            [this](const sfg_agent_msgs::msg::DiscoveryEvent::ConstSharedPtr &msg)
            {
                agent_discovery_event_callback(msg->metadata, msg->event_type);
            });

        m_get_discovered_agents_client = create_client<sfg_agent_msgs::srv::GetDiscoveredAgents>(RosFqnBuilder().scope(Scope::Local).agent().resource(Resource::Custom, "get_discovered_agents").build());

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

        // ToDo: Perhaps we should query the currently loaded nodes in the container and populate m_decoded_agents accordingly.
        // This would allow the agent decoder to recover from a crash. Although, I think if the agent decoder crashes, the container
        // would also crash, so perhaps this is not necessary.
    }

    void AgentDecoder::agent_discovery_event_callback(const sfg_agent_msgs::msg::Metadata &metadata, uint8_t event_type)
    {
        auto agent_name = metadata.agent_name;

        if (!std::regex_match(agent_name, m_compiled_agent_name_regex))
        {
            return;
        }

        auto iterator = m_decoded_agents.find(agent_name);

        switch (event_type)
        {
        case sfg_agent_msgs::msg::DiscoveryEvent::DISCOVERED:
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
        case sfg_agent_msgs::msg::DiscoveryEvent::LOST:
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
            agent_discovery_event_callback(metadata, sfg_agent_msgs::msg::DiscoveryEvent::DISCOVERED);
        }
    }

    void AgentDecoder::load_nodes(
        std::shared_ptr<DecodedAgent> agent,
        const sfg_agent_msgs::msg::Metadata &metadata)
    {
        using namespace sfg_utils::fqn;

        auto weak_agent = std::weak_ptr<DecodedAgent>(agent);

        for (const auto &camera : metadata.cameras)
        {
            for (auto stream : {Stream::Color, Stream::Depth})
            {
                for (auto request : {create_load_camera_decoder_request(metadata.agent_name, camera, stream), create_load_camera_info_relay_request(metadata.agent_name, camera, stream)})
                {
                    auto package_name = request->package_name;
                    auto plugin_name = request->plugin_name;

                    m_load_node_client->async_send_request(
                        request, [this, weak_agent, package_name, plugin_name](rclcpp::Client<composition_interfaces::srv::LoadNode>::SharedFuture future)
                        { load_node_callback(weak_agent, package_name, plugin_name, future); });
                }
            }
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
            agent->m_loaded_nodes.push_back(std::make_tuple(package_name, plugin_name, id));
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
        for (const auto &[package_name, plugin_name, id] : agent->m_loaded_nodes)
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

    std::shared_ptr<composition_interfaces::srv::LoadNode::Request>
    AgentDecoder::create_load_camera_decoder_request(const std::string &agent_name, const std::string &camera, sfg_utils::fqn::Stream stream)
    {
        using namespace sfg_utils::fqn;

        assert(stream == Stream::Color || stream == Stream::Depth);

        auto agent_fqn_builder = RosFqnBuilder().scope(Scope::Global).agent(agent_name).component(Component::Camera, camera).stream(stream);
        RCLCPP_INFO(get_logger(), "Adding %s camera decoder for '%s' for agent '%s'.", magic_enum::enum_name(stream).data(), agent_fqn_builder.build(RosFqnSegment::Component).c_str(), agent_name.c_str());

        auto input_topic = agent_fqn_builder.resource(Resource::ImageCompressed).build();
        auto output_topic = agent_fqn_builder.scope(Scope::Local).resource(Resource::ImageRaw).build();

        std::string in_transport = (stream == Stream::Color ? "ffmpeg" : "compressedDepth");
        std::string out_transport = "raw";

        auto request = std::make_shared<composition_interfaces::srv::LoadNode::Request>();
        request->package_name = "sfg_image_transport";
        request->plugin_name = request->package_name + "::Republisher";
        request->node_name = agent_fqn_builder.build(RosFqnSegment::Component) + (stream == Stream::Color ? "_color" : "_depth") + "_decoder";
        request->node_namespace = agent_fqn_builder.build(RosFqnSegment::Scope, RosFqnSegment::Agent);
        request->parameters = m_camera_decoder_parameters;
        request->parameters.push_back(rclcpp::Parameter("in_transport", in_transport).to_parameter_msg());
        request->parameters.push_back(rclcpp::Parameter("out_transport", out_transport).to_parameter_msg());
        request->extra_arguments = {rclcpp::Parameter("use_intra_process_comms", get_node_options().use_intra_process_comms()).to_parameter_msg()};
        request->remap_rules = {
            "in/" + in_transport + ":=" + input_topic,
            "out:=" + output_topic};

        return request;
    }

    std::shared_ptr<composition_interfaces::srv::LoadNode::Request>
    AgentDecoder::create_load_camera_info_relay_request(const std::string &agent_name, const std::string &camera, sfg_utils::fqn::Stream stream)
    {
        using namespace sfg_utils::fqn;

        assert(stream == Stream::Color || stream == Stream::Depth);

        auto agent_fqn_builder = RosFqnBuilder().scope(Scope::Global).agent(agent_name).component(Component::Camera, camera).stream(stream);
        RCLCPP_INFO(get_logger(), "Adding %s camera info relay for '%s' for agent '%s'.", magic_enum::enum_name(stream).data(), agent_fqn_builder.build(RosFqnSegment::Component).c_str(), agent_name.c_str());

        auto input_topic = agent_fqn_builder.resource(Resource::CameraInfo).build();
        auto output_topic = agent_fqn_builder.scope(Scope::Local).build();

        auto request = std::make_shared<composition_interfaces::srv::LoadNode::Request>();
        request->package_name = "sfg_topic_tools";
        request->plugin_name = request->package_name + "::Relay";
        request->node_name = agent_fqn_builder.build(RosFqnSegment::Component) + (stream == Stream::Color ? "_color" : "_depth") + "_info_relay";
        request->node_namespace = agent_fqn_builder.build(RosFqnSegment::Scope, RosFqnSegment::Agent);
        request->parameters = m_camera_info_relay_parameters;
        request->parameters.push_back(rclcpp::Parameter("input_topic", input_topic).to_parameter_msg());
        request->parameters.push_back(rclcpp::Parameter("output_topic", output_topic).to_parameter_msg());
        request->parameters.push_back(rclcpp::Parameter("msg_type", "sensor_msgs/msg/CameraInfo").to_parameter_msg());
        request->parameters.push_back(rclcpp::Parameter("qos_overrides." + input_topic + ".subscription.reliability", "best_effort").to_parameter_msg());
        request->parameters.push_back(rclcpp::Parameter("qos_overrides." + output_topic + ".publisher.reliability", "best_effort").to_parameter_msg());
        request->extra_arguments = {rclcpp::Parameter("use_intra_process_comms", get_node_options().use_intra_process_comms()).to_parameter_msg()};

        return request;
    }
}