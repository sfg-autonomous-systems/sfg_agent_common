#pragma once

#include <composition_interfaces/srv/load_node.hpp>
#include <composition_interfaces/srv/unload_node.hpp>
#include <map>
#include <mutex>
#include <rclcpp/rclcpp.hpp>
#include <regex>

#include "sfg_agent_msgs/msg/agent_heartbeat.hpp"
#include "sfg_agent_msgs/srv/get_agent_metadata.hpp"

namespace sfg_agent
{
    class AgentDecoder : public rclcpp::Node
    {
    public:
        AgentDecoder(const rclcpp::NodeOptions &options);

    private:
        struct AgentData
        {
        public:
            std::string m_hostname;
            std::string m_sanitized_hostname;
            rclcpp::TimerBase::SharedPtr m_keepalive_timer;
            rclcpp::Client<sfg_agent_msgs::srv::GetAgentMetadata>::SharedPtr m_get_metadata_client;
            std::vector<uint64_t> m_loaded_node_ids;
        };

        void heartbeat_callback(const sfg_agent_msgs::msg::AgentHeartbeat::SharedPtr msg);
        void keepalive_callback(
            const std::weak_ptr<AgentData> weak_agent_data);
        void agent_metadata_callback(
            const std::weak_ptr<AgentData> weak_agent_data,
            rclcpp::Client<sfg_agent_msgs::srv::GetAgentMetadata>::SharedFuture future);
        void load_node_callback(
            const std::weak_ptr<AgentData> weak_agent_data,
            const std::string &package_name,
            const std::string &plugin_name,
            rclcpp::Client<composition_interfaces::srv::LoadNode>::SharedFuture future);
        void unload_node_callback(
            uint64_t id,
            rclcpp::Client<composition_interfaces::srv::UnloadNode>::SharedFuture future);

        void load_node(
            const std::shared_ptr<AgentData> agent_data,
            const std::string &package_name,
            const std::string &plugin_name,
            const std::string &node_name,
            const std::vector<std::string> &remapping_rules);
        void unload_node(uint64_t id);

        // ROS parameters
        std::string m_container_name;
        std::string m_hostname_regex;
        uint8_t m_keepalive;

        std::regex m_compiled_hostname_regex;
        std::map<std::string, std::shared_ptr<AgentData>> m_decoded_agents;
        rclcpp::CallbackGroup::SharedPtr m_callback_group;
        std::mutex m_mutex;

        rclcpp::Subscription<sfg_agent_msgs::msg::AgentHeartbeat>::SharedPtr m_agent_heartbeat_subscriber;
        rclcpp::Client<composition_interfaces::srv::LoadNode>::SharedPtr m_load_node_client;
        rclcpp::Client<composition_interfaces::srv::UnloadNode>::SharedPtr m_unload_node_client;
    };
}