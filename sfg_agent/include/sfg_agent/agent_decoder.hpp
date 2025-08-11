#pragma once

#include <composition_interfaces/srv/load_node.hpp>
#include <composition_interfaces/srv/unload_node.hpp>
#include <rclcpp/rclcpp.hpp>
#include <regex>

#include "sfg_agent_msgs/msg/agent_discovery_event.hpp"
#include "sfg_agent_msgs/msg/agent_metadata.hpp"
#include "sfg_agent_msgs/srv/get_discovered_agents.hpp"
#include "sfg_utils/fqn/stream.hpp"

namespace sfg_agent
{
    class AgentDecoder : public rclcpp::Node
    {
    public:
        AgentDecoder(const rclcpp::NodeOptions &options);

    private:
        struct DecodedAgent
        {
        public:
            std::vector<std::tuple<std::string, std::string, uint64_t>> m_loaded_decoders;
        };

        void handle_agent_disovery_event(
            const sfg_agent_msgs::msg::AgentMetadata &metadata,
            uint8_t event_type);
        void get_discovered_agents_callback(rclcpp::Client<sfg_agent_msgs::srv::GetDiscoveredAgents>::SharedFuture future);

        void load_nodes(
            std::shared_ptr<DecodedAgent> agent,
            const sfg_agent_msgs::msg::AgentMetadata &metadata);
        void load_node_callback(
            std::weak_ptr<DecodedAgent> weak_agent,
            const std::string &package_name,
            const std::string &plugin_name,
            rclcpp::Client<composition_interfaces::srv::LoadNode>::SharedFuture future);

        void unload_nodes(std::shared_ptr<DecodedAgent> agent);
        void unload_node_callback(
            const std::string &package_name,
            const std::string &plugin_name,
            uint64_t id,
            rclcpp::Client<composition_interfaces::srv::UnloadNode>::SharedFuture future);

        std::shared_ptr<composition_interfaces::srv::LoadNode::Request> create_load_camera_decoder_node_request(const std::string &agent_name, const std::string &camera, sfg_utils::fqn::Stream stream);

        // ROS parameters
        std::string m_container_name;
        std::string m_agent_name_regex;

        std::regex m_compiled_agent_name_regex;
        std::map<std::string, std::shared_ptr<DecodedAgent>> m_decoded_agents;
        std::vector<rcl_interfaces::msg::Parameter> m_parameters;

        rclcpp::Subscription<sfg_agent_msgs::msg::AgentDiscoveryEvent>::SharedPtr m_agent_discovery_event_subscriber;
        rclcpp::Client<sfg_agent_msgs::srv::GetDiscoveredAgents>::SharedPtr m_get_discovered_agents_client;
        rclcpp::Client<composition_interfaces::srv::LoadNode>::SharedPtr m_load_node_client;
        rclcpp::Client<composition_interfaces::srv::UnloadNode>::SharedPtr m_unload_node_client;
    };
}