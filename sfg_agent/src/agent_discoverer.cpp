#include "sfg_agent/agent_discoverer.hpp"

#include "sfg_agent/agent_heartbeat_constants.hpp"
#include "sfg_utils/sanitize_hostname.hpp"

namespace sfg_agent
{
    AgentDiscoverer::AgentDiscoverer(const rclcpp::NodeOptions &options)
        : Node("agent_discovery_server", options)
    {
        // Declare and retrieve ROS parameters.
        std::string parameter = "keepalive";
        declare_parameter(
            parameter,
            3,
            rcl_interfaces::msg::ParameterDescriptor()
                .set__description("The number of heartbeat messages to wait before considering an agent dead."));
        get_parameter(parameter, m_keepalive);

        // Set up interfaces.
        m_heartbeat_subscriber = create_subscription<sfg_agent_msgs::msg::AgentHeartbeat>(
            "/global/agent_heartbeat",
            rclcpp::QoS(10).best_effort(),
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
        auto hostname = msg->hostname;
        auto iterator = m_discovered_agents.find(hostname);

        if (iterator != m_discovered_agents.end())
        {
            iterator->second->m_keepalive_timer->reset();
            return;
        }

        // Check if the agent has a pending metadata request.
        if (m_pending_get_metadata_requests.find(hostname) != m_pending_get_metadata_requests.end())
        {
            RCLCPP_WARN(get_logger(), "Agent '%s' has an already pending metadata request.", hostname.c_str());
            return;
        }

        auto metadata_client = m_pending_get_metadata_requests[hostname] = create_client<sfg_agent_msgs::srv::GetMetadata>(
            "/global/" + sfg_utils::sanitize_hostname(hostname) + "/get_metadata",
            rmw_qos_profile_services_default);

        if (!metadata_client->service_is_ready())
        {
            RCLCPP_WARN(get_logger(), "Get agent metadata service for '%s' not ready.", hostname.c_str());
            return;
        }

        RCLCPP_INFO(get_logger(), "Requesting agent metadata for '%s'.", hostname.c_str());

        metadata_client->async_send_request(
            std::make_shared<sfg_agent_msgs::srv::GetMetadata::Request>(),
            [this, hostname](rclcpp::Client<sfg_agent_msgs::srv::GetMetadata>::SharedFuture future)
            {
                get_metadata_callback(hostname, future);
            });
    }

    void AgentDiscoverer::keepalive_callback(const std::string &hostname)
    {
        auto iterator = m_discovered_agents.find(hostname);

        if (iterator == m_discovered_agents.end())
        {
            RCLCPP_WARN(get_logger(), "Agent '%s' has already been lost.", hostname.c_str());
            return;
        }

        auto msg = sfg_agent_msgs::msg::AgentDiscoveryEvent();
        msg.header.stamp = now();
        msg.metadata = iterator->second->m_metadata;
        msg.event_type = sfg_agent_msgs::msg::AgentDiscoveryEvent::LOST;
        m_agent_discovery_event_publisher->publish(msg);

        m_discovered_agents.erase(hostname);
        RCLCPP_INFO(get_logger(), "Agent '%s' lost.", hostname.c_str());
    }

    void AgentDiscoverer::get_metadata_callback(
        const std::string &hostname,
        rclcpp::Client<sfg_agent_msgs::srv::GetMetadata>::SharedFuture future)
    {
        m_pending_get_metadata_requests.erase(hostname);

        if (!future.valid())
        {
            RCLCPP_ERROR(get_logger(), "Failed to get metadata for agent '%s': Future is invalid.", hostname.c_str());
            return;
        }

        rclcpp::Client<sfg_agent_msgs::srv::GetMetadata>::SharedResponse response;

        try
        {
            response = future.get();
        }
        catch (const std::exception &exception)
        {
            RCLCPP_ERROR(get_logger(), "Failed to get metadata for agent '%s': %s", hostname.c_str(), exception.what());
            return;
        }

        RCLCPP_INFO(get_logger(), "Agent '%s' discovered.", hostname.c_str());
        auto agent = m_discovered_agents[hostname] = std::make_shared<DiscoveredAgent>();

        agent->m_keepalive_timer = create_wall_timer(
            std::chrono::seconds(m_keepalive * AGENT_HEARTBEAT_INTERVAL),
            [this, hostname]()
            {
                keepalive_callback(hostname);
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
