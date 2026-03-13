#pragma once

#include <regex>
#include <thread>

#include "sfg_agent_msgs/msg/discovery_event.hpp"
#include "sfg_agent_msgs/msg/metadata.hpp"
#include "sfg_agent_msgs/srv/get_discovered_agents.hpp"
#include "sfg_composition_interfaces/lazy_composable_node_loader.hpp"
#include "sfg_utils/fqn/stream.hpp"

namespace sfg_agent
{
    class AgentDecoder : public rclcpp::Node
    {
    public:
        AgentDecoder(const rclcpp::NodeOptions &options);
        ~AgentDecoder();

    private:
        struct Agent
        {
        public:
            sfg_agent_msgs::msg::Metadata m_metadata;
            std::vector<std::shared_ptr<sfg_composition_interfaces::LazyComposableNodeLoader>> m_decoders;
        };

        AgentDecoder &operator=(const AgentDecoder &) = default;
        AgentDecoder(const AgentDecoder &) = default;
        AgentDecoder(AgentDecoder &&) = default;
        AgentDecoder &operator=(AgentDecoder &&) = default;

        void listen_to_graph_events();
        void agent_discovery_event_callback(const sfg_agent_msgs::msg::DiscoveryEvent::ConstSharedPtr msg);
        void get_discovered_agents_callback(rclcpp::Client<sfg_agent_msgs::srv::GetDiscoveredAgents>::SharedFuture future);
        std::shared_ptr<sfg_composition_interfaces::LazyComposableNodeLoader> create_camera_decoder(const std::string &agent_name, const std::string &camera, sfg_utils::fqn::Stream stream);
        std::shared_ptr<sfg_composition_interfaces::LazyComposableNodeLoader> create_camera_info_decoder(const std::string &agent_name, const std::string &camera, sfg_utils::fqn::Stream stream);

        // ROS parameters
        std::string m_container_name;
        std::vector<rcl_interfaces::msg::Parameter> m_camera_decoder_parameters;
        std::vector<rcl_interfaces::msg::Parameter> m_camera_info_relay_parameters;

        std::thread m_thread;
        std::atomic<bool> m_done = false;
        std::mutex m_agents_mutex;
        std::unordered_map<std::string, Agent> m_agents;

        rclcpp::Subscription<sfg_agent_msgs::msg::DiscoveryEvent>::SharedPtr m_agent_discovery_event_subscriber;
        rclcpp::Client<sfg_agent_msgs::srv::GetDiscoveredAgents>::SharedPtr m_get_discovered_agents_client;
        rclcpp::Client<composition_interfaces::srv::LoadNode>::SharedPtr m_load_node_client;
        rclcpp::Client<composition_interfaces::srv::UnloadNode>::SharedPtr m_unload_node_client;
    };
}