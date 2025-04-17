#pragma once

#include <cstdint>
#include <filesystem>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/timer.hpp>

#include "sfg_agent_msgs/msg/agent_heartbeat.hpp"
#include "sfg_agent_msgs/srv/get_agent_metadata.hpp"

namespace sfg_agent
{
    class AgentStatusProvider : public rclcpp::Node
    {
    public:
        AgentStatusProvider();

    private:
        void publish_heartbeat();
        void get_metadata(const std::shared_ptr<sfg_agent_msgs::srv::GetAgentMetadata::Request> request,
                          std::shared_ptr<sfg_agent_msgs::srv::GetAgentMetadata::Response> response);
        bool load_metadata(const std::filesystem::path &filepath);
        std::string get_sanitized_hostname();

        // ROS parameters
        std::string m_override_hostname;
        std::string m_metadata_filepath;

        std::string m_hostname;
        rclcpp::Publisher<sfg_agent_msgs::msg::AgentHeartbeat>::SharedPtr m_heartbeat_publisher;
        rclcpp::TimerBase::SharedPtr m_heartbeat_timer;
        rclcpp::Service<sfg_agent_msgs::srv::GetAgentMetadata>::SharedPtr m_metadata_service;
        sfg_agent_msgs::srv::GetAgentMetadata::Response m_metadata_response;
    };
}