#pragma once

#include <rclcpp/rclcpp.hpp>

#include "sfg_agent_msgs/msg/agent_heartbeat.hpp"
#include "sfg_agent_msgs/srv/get_agent_metadata.hpp"

namespace sfg_agent
{
    class AgentDecoder : public rclcpp::Node
    {
    public:
        AgentDecoder(rclcpp::Executor &executor);

    private:
        void heartbeat_callback(const sfg_agent_msgs::msg::AgentHeartbeat::SharedPtr msg);
        void keepalive_callback();
        void agent_metadata_callback(
            rclcpp::Client<sfg_agent_msgs::srv::GetAgentMetadata>::SharedFuture future);

        // ROS parameters
        std::string m_hostname;
        uint8_t m_keepalive;

        rclcpp::Executor &m_executor;
        uint8_t m_keepalive_count;
        rclcpp::TimerBase::SharedPtr m_keepalive_timer;
        std::vector<rclcpp::Node::SharedPtr> m_decoder_nodes;
        rclcpp::Subscription<sfg_agent_msgs::msg::AgentHeartbeat>::SharedPtr m_heartbeat_subscriber;
        rclcpp::Client<sfg_agent_msgs::srv::GetAgentMetadata>::SharedPtr m_agent_metadata_client;
    };
}