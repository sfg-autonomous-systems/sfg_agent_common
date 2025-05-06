#include "sfg_agent/agent_discoverer.hpp"

#include "sfg_agent/agent_heartbeat_constants.hpp"
#include "sfg_utils/get_agent_name.hpp"
#include "sfg_utils/sanitize_agent_name.hpp"

namespace sfg_agent
{
    AgentDiscoverer::AgentDiscoverer(const rclcpp::NodeOptions &options)
        : Node("agent_discoverer", options)
    {
        // Declare and retrieve ROS parameters.
        std::string parameter = "keepalive";
        declare_parameter(
            parameter,
            3,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The number of heartbeat messages to wait before considering an agent dead."));
        get_parameter(parameter, m_keepalive);

        parameter = "exclude_self";
        declare_parameter(
            parameter,
            true,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("Whether to exclude the local agent from the list of discovered agents."));
        get_parameter(parameter, m_exclude_self);

        m_agent_name = sfg_utils::get_agent_name();

        // Set up interfaces.
        m_heartbeat_subscriber = create_subscription<sfg_agent_msgs::msg::AgentHeartbeat>(
            "/global/agent_heartbeat",
            rclcpp::QoS(10).reliable(),
            std::bind(&AgentDiscoverer::heartbeat_callback, this, std::placeholders::_1));
        m_agent_discovery_event_publisher = create_publisher<sfg_agent_msgs::msg::AgentDiscoveryEvent>(
            "agent_discovery_event",
            rclcpp::QoS(rclcpp::KeepAll()).reliable());
        m_get_discovered_agents_service = create_service<sfg_agent_msgs::srv::GetDiscoveredAgents>(
            "get_discovered_agents",
            std::bind(&AgentDiscoverer::get_discovered_agents_callback, this, std::placeholders::_1, std::placeholders::_2));

        RCLCPP_INFO(get_logger(), "Started agent discovery server.");
    }

    void AgentDiscoverer::heartbeat_callback(const sfg_agent_msgs::msg::AgentHeartbeat::SharedPtr msg)
    {
        auto agent_name = msg->agent_name;

        // Check if the agent is already discovered.
        if (m_exclude_self && agent_name == m_agent_name)
        {
            return;
        }

        auto iterator = m_discovered_agents.find(agent_name);

        if (iterator != m_discovered_agents.end())
        {
            iterator->second->m_keepalive_timer->reset();
            return;
        }

        // Check if the agent has a pending metadata request.
        if (m_pending_get_metadata_requests.find(agent_name) != m_pending_get_metadata_requests.end())
        {
            RCLCPP_WARN(get_logger(), "Agent '%s' has an already pending metadata request.", agent_name.c_str());
            return;
        }

        auto metadata_client = m_pending_get_metadata_requests[agent_name] = create_client<sfg_agent_msgs::srv::GetMetadata>(
            "/global/" + sfg_utils::sanitize_agent_name(agent_name) + "/get_metadata",
            rmw_qos_profile_services_default);

        if (!metadata_client->service_is_ready())
        {
            RCLCPP_WARN(get_logger(), "Get agent metadata service for '%s' not ready.", agent_name.c_str());
            return;
        }

        RCLCPP_INFO(get_logger(), "Requesting agent metadata for '%s'.", agent_name.c_str());

        metadata_client->async_send_request(
            std::make_shared<sfg_agent_msgs::srv::GetMetadata::Request>(),
            [this, agent_name](rclcpp::Client<sfg_agent_msgs::srv::GetMetadata>::SharedFuture future)
            {
                get_metadata_callback(agent_name, future);
            });
    }

    void AgentDiscoverer::keepalive_callback(const std::string &agent_name)
    {
        auto iterator = m_discovered_agents.find(agent_name);

        if (iterator == m_discovered_agents.end())
        {
            RCLCPP_WARN(get_logger(), "Agent '%s' has already been lost.", agent_name.c_str());
            return;
        }

        auto msg = sfg_agent_msgs::msg::AgentDiscoveryEvent();
        msg.header.stamp = now();
        msg.metadata = iterator->second->m_metadata;
        msg.event_type = sfg_agent_msgs::msg::AgentDiscoveryEvent::LOST;
        m_agent_discovery_event_publisher->publish(msg);

        m_discovered_agents.erase(agent_name);
        RCLCPP_INFO(get_logger(), "Agent '%s' lost.", agent_name.c_str());
    }

    void AgentDiscoverer::get_metadata_callback(
        const std::string &agent_name,
        rclcpp::Client<sfg_agent_msgs::srv::GetMetadata>::SharedFuture future)
    {
        m_pending_get_metadata_requests.erase(agent_name);

        if (!future.valid())
        {
            RCLCPP_ERROR(get_logger(), "Failed to get metadata for agent '%s': Future is invalid.", agent_name.c_str());
            return;
        }

        rclcpp::Client<sfg_agent_msgs::srv::GetMetadata>::SharedResponse response;

        try
        {
            response = future.get();
        }
        catch (const std::exception &exception)
        {
            RCLCPP_ERROR(get_logger(), "Failed to get metadata for agent '%s': %s", agent_name.c_str(), exception.what());
            return;
        }

        RCLCPP_INFO(get_logger(), "Agent '%s' discovered.", agent_name.c_str());
        auto agent = m_discovered_agents[agent_name] = std::make_shared<DiscoveredAgent>();

        agent->m_keepalive_timer = create_wall_timer(
            std::chrono::seconds(m_keepalive * AGENT_HEARTBEAT_INTERVAL),
            [this, agent_name]()
            {
                keepalive_callback(agent_name);
            });
        agent->m_metadata = response->metadata;

        // Inform subscribers about the new agent.
        auto msg = sfg_agent_msgs::msg::AgentDiscoveryEvent();
        msg.header.stamp = now();
        msg.metadata = response->metadata;
        msg.event_type = sfg_agent_msgs::msg::AgentDiscoveryEvent::DISCOVERED;
        m_agent_discovery_event_publisher->publish(msg);
    }

    void sfg_agent::AgentDiscoverer::get_discovered_agents_callback(
        [[maybe_unused]] const std::shared_ptr<sfg_agent_msgs::srv::GetDiscoveredAgents::Request> request,
        std::shared_ptr<sfg_agent_msgs::srv::GetDiscoveredAgents::Response> response)
    {
        RCLCPP_INFO(get_logger(), "Received request for discovered agents metadata.");

        for (const auto &agent : m_discovered_agents)
        {
            response->metadata.push_back(agent.second->m_metadata);
        }
    }
}
