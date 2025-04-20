#pragma once

#include <rclcpp/rclcpp.hpp>
#include <composition_interfaces/srv/load_node.hpp>
#include <composition_interfaces/srv/unload_node.hpp>

#include "sfg_agent_msgs/msg/agent_heartbeat.hpp"
#include "sfg_agent_msgs/srv/get_agent_metadata.hpp"

namespace sfg_agent
{
    class AgentDecoder : public rclcpp::Node
    {
    public:
        AgentDecoder(const rclcpp::NodeOptions &options);

    private:
        void heartbeat_callback(const sfg_agent_msgs::msg::AgentHeartbeat::SharedPtr msg);
        void keepalive_callback();
        void agent_metadata_callback(
            rclcpp::Client<sfg_agent_msgs::srv::GetAgentMetadata>::SharedFuture future);
        void load_node(const std::string &package_name, const std::string &plugin_name);
        void unload_node(const uint64_t id);

        // ROS parameters
        std::string m_hostname;
        uint8_t m_keepalive;

        uint8_t m_keepalive_count;
        rclcpp::TimerBase::SharedPtr m_keepalive_timer;
        std::string m_container_manager_name;
        std::vector<uint64_t> m_loaded_node_ids;

        rclcpp::Subscription<sfg_agent_msgs::msg::AgentHeartbeat>::SharedPtr m_agent_heartbeat_subscriber;
        rclcpp::Client<sfg_agent_msgs::srv::GetAgentMetadata>::SharedPtr m_get_agent_metadata_client;
        rclcpp::Client<composition_interfaces::srv::LoadNode>::SharedPtr m_load_node_client;
        rclcpp::Client<composition_interfaces::srv::UnloadNode>::SharedPtr m_unload_node_client;
    };
}