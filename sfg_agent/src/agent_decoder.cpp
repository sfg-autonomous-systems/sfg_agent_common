#include "sfg_agent/agent_decoder.hpp"

#include <magic_enum.hpp>

#include "sfg_utils/fqn/ros_fqn_builder.hpp"
#include "sfg_utils/ros_utils.hpp"

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

        m_camera_decoder_parameters = sfg_utils::ros_utils::extract_parameters<rcl_interfaces::msg::Parameter>(*this, "camera_decoder_parameters");
        m_camera_info_relay_parameters = sfg_utils::ros_utils::extract_parameters<rcl_interfaces::msg::Parameter>(*this, "camera_info_relay_parameters");

        using namespace sfg_utils::fqn;

        // Set up interfaces.
        m_agent_discovery_event_subscriber = create_subscription<sfg_agent_msgs::msg::DiscoveryEvent>(
            RosFqnBuilder().scope(Scope::Local).agent().resource(Resource::Custom, "agent_discovery_event").build(),
            10,
            std::bind(&AgentDecoder::agent_discovery_event_callback, this, std::placeholders::_1));

        m_get_discovered_agents_client = create_client<sfg_agent_msgs::srv::GetDiscoveredAgents>(RosFqnBuilder().scope(Scope::Local).agent().resource(Resource::Custom, "get_discovered_agents").build());

        std::string load_node_service = m_container_name + "/_container/load_node";
        RCLCPP_INFO(get_logger(), "Creating client for service '%s'.", load_node_service.c_str());
        m_load_node_client = create_client<composition_interfaces::srv::LoadNode>(load_node_service);
        std::string unload_node_service = m_container_name + "/_container/unload_node";
        RCLCPP_INFO(get_logger(), "Creating client for service '%s'.", unload_node_service.c_str());
        m_unload_node_client = create_client<composition_interfaces::srv::UnloadNode>(unload_node_service);

        m_thread = std::thread(&AgentDecoder::listen_to_graph_events, this);
        auto request = std::make_shared<sfg_agent_msgs::srv::GetDiscoveredAgents::Request>();
        m_get_discovered_agents_client->async_send_request(request, std::bind(&AgentDecoder::get_discovered_agents_callback, this, std::placeholders::_1));

        RCLCPP_INFO(get_logger(), "Started agent decoder.");
        // ToDo: Perhaps we should query the currently loaded nodes in the container and populate m_decoded_agents accordingly.
        // This would allow the agent decoder to recover from a crash. Although, I think if the agent decoder crashes, the container
        // would also crash, so perhaps this is not necessary.
    }

    AgentDecoder::~AgentDecoder()
    {
        m_done = true;
        // Manually trigger a graph change to unblock the graph listener thread if it's currently waiting.
        get_node_graph_interface()->notify_graph_change();

        if (m_thread.joinable())
        {
            m_thread.join();
        }

        for (auto &[agent_name, agent] : m_agents)
        {
            for (auto &decoder : agent.m_decoders)
            {
                decoder->unload();
            }
        }
    }

    void AgentDecoder::listen_to_graph_events()
    {
        while (rclcpp::ok() && !m_done)
        {
            auto event = get_graph_event();
            wait_for_graph_change(event, std::chrono::seconds(1));

            if (!rclcpp::ok() || m_done)
            {
                break;
            }

            std::lock_guard lock(m_agents_mutex);

            for (const auto &[agent_name, agent] : m_agents)
            {
                for (const auto &decoder : agent.m_decoders)
                {
                    // ToDo: Implement some sort of delayed unloading.
                    // ToDo: Verify that count_subscribers respects intra-process subscribers.
                    count_subscribers(decoder->get_output_topic()) > 0 ? decoder->load() : decoder->unload();
                }
            }
        }
    }

    void AgentDecoder::agent_discovery_event_callback(const sfg_agent_msgs::msg::DiscoveryEvent::ConstSharedPtr msg)
    {
        std::lock_guard lock(m_agents_mutex);
        auto agent_name = msg->metadata.agent_name;
        auto iterator = m_agents.find(agent_name);

        switch (msg->event_type)
        {
            case sfg_agent_msgs::msg::DiscoveryEvent::DISCOVERED:
            {
                if (iterator != m_agents.end())
                {
                    return;
                }
                auto agent = Agent{msg->metadata, {}};

                for (const auto &camera : msg->metadata.cameras)
                {
                    for (auto stream : {sfg_utils::fqn::Stream::Color, sfg_utils::fqn::Stream::Depth})
                    {
                        agent.m_decoders.push_back(create_camera_decoder(agent_name, camera, stream));
                        agent.m_decoders.push_back(create_camera_info_decoder(agent_name, camera, stream));
                    }
                }
                m_agents.emplace(agent_name, std::move(agent));
                break;
            }
            case sfg_agent_msgs::msg::DiscoveryEvent::LOST:
            {
                if (iterator == m_agents.end())
                {
                    return;
                }
                auto &agent = iterator->second;

                for (auto &decoder : agent.m_decoders)
                {
                    decoder->unload();
                }
                m_agents.erase(iterator);
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
            auto msg = std::make_shared<sfg_agent_msgs::msg::DiscoveryEvent>();
            msg->header.stamp = now();
            msg->metadata = metadata;
            msg->event_type = sfg_agent_msgs::msg::DiscoveryEvent::DISCOVERED;
            agent_discovery_event_callback(msg);
        }
    }

    std::shared_ptr<sfg_composition_interfaces::LazyComposableNodeLoader> AgentDecoder::create_camera_decoder(const std::string &agent_name, const std::string &camera, sfg_utils::fqn::Stream stream)
    {
        using namespace sfg_utils::fqn;
        assert(stream == Stream::Color || stream == Stream::Depth);

        auto fqn_builder = RosFqnBuilder().scope(Scope::Global).agent(agent_name).component(Component::Camera, camera).stream(stream);
        auto input_topic = fqn_builder.resource(Resource::ImageCompressed).build();
        auto output_topic = fqn_builder.scope(Scope::Local).resource(Resource::ImageRaw).build();

        std::string in_transport = (stream == Stream::Color ? "ffmpeg" : "compressedDepth");
        std::string out_transport = "raw";

        auto request = std::make_shared<composition_interfaces::srv::LoadNode::Request>();
        request->package_name = "sfg_image_transport";
        request->plugin_name = request->package_name + "::Republisher";
        request->node_name = fqn_builder.build(RosFqnSegment::Component) + (stream == Stream::Color ? "_color" : "_depth") + "_decoder";
        request->node_namespace = fqn_builder.build(RosFqnSegment::Scope, RosFqnSegment::Agent);
        request->parameters = m_camera_decoder_parameters;
        request->parameters.push_back(rclcpp::Parameter("in_transport", in_transport).to_parameter_msg());
        request->parameters.push_back(rclcpp::Parameter("out_transport", out_transport).to_parameter_msg());
        request->extra_arguments = {rclcpp::Parameter("use_intra_process_comms", get_node_options().use_intra_process_comms()).to_parameter_msg()};
        request->remap_rules = {
            "in/" + in_transport + ":=" + input_topic,
            "out:=" + output_topic};

        return std::make_shared<sfg_composition_interfaces::LazyComposableNodeLoader>(output_topic, request, m_load_node_client, m_unload_node_client, get_logger());
    }

    std::shared_ptr<sfg_composition_interfaces::LazyComposableNodeLoader> AgentDecoder::create_camera_info_decoder(const std::string &agent_name, const std::string &camera, sfg_utils::fqn::Stream stream)
    {
        using namespace sfg_utils::fqn;
        assert(stream == Stream::Color || stream == Stream::Depth);

        auto fqn_builder = RosFqnBuilder().scope(Scope::Global).agent(agent_name).component(Component::Camera, camera).stream(stream);
        auto input_topic = fqn_builder.resource(Resource::CameraInfo).build();
        auto output_topic = fqn_builder.scope(Scope::Local).resource(Resource::CameraInfo).build();

        auto request = std::make_shared<composition_interfaces::srv::LoadNode::Request>();
        request->package_name = "sfg_topic_tools";
        request->plugin_name = request->package_name + "::Relay";
        request->node_name = fqn_builder.build(RosFqnSegment::Component) + (stream == Stream::Color ? "_color" : "_depth") + "_info_relay";
        request->node_namespace = fqn_builder.build(RosFqnSegment::Scope, RosFqnSegment::Agent);
        request->parameters = m_camera_info_relay_parameters;
        request->parameters.push_back(rclcpp::Parameter("input_topic", input_topic).to_parameter_msg());
        request->parameters.push_back(rclcpp::Parameter("output_topic", output_topic).to_parameter_msg());
        request->parameters.push_back(rclcpp::Parameter("msg_type", "sensor_msgs/msg/CameraInfo").to_parameter_msg());
        request->parameters.push_back(rclcpp::Parameter("qos_overrides." + input_topic + ".subscription.reliability", "best_effort").to_parameter_msg());
        request->parameters.push_back(rclcpp::Parameter("qos_overrides." + output_topic + ".publisher.reliability", "best_effort").to_parameter_msg());
        request->extra_arguments = {rclcpp::Parameter("use_intra_process_comms", get_node_options().use_intra_process_comms()).to_parameter_msg()};

        return std::make_shared<sfg_composition_interfaces::LazyComposableNodeLoader>(output_topic, request, m_load_node_client, m_unload_node_client, get_logger());
    }
}