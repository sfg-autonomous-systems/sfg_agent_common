#pragma once

#include <cstdint>
#include <filesystem>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/timer.hpp>

#include "sfg_agent_msgs/msg/agent_heartbeat.hpp"
#include "sfg_agent_msgs/srv/get_metadata.hpp"

namespace sfg_agent
{
    class AgentStatusProvider : public rclcpp::Node
    {
    public:
        AgentStatusProvider(const rclcpp::NodeOptions &options);

    private:
        void publish_heartbeat();
        void get_metadata_callback(
            const std::shared_ptr<sfg_agent_msgs::srv::GetMetadata::Request> request,
            std::shared_ptr<sfg_agent_msgs::srv::GetMetadata::Response> response);
        bool load_metadata(const std::filesystem::path &filepath);

        // ROS parameters
        std::string m_metadata_filepath;

        std::string m_agent_name;

        rclcpp::Publisher<sfg_agent_msgs::msg::AgentHeartbeat>::SharedPtr m_heartbeat_publisher;
        rclcpp::TimerBase::SharedPtr m_heartbeat_timer;
        rclcpp::Service<sfg_agent_msgs::srv::GetMetadata>::SharedPtr m_get_metadata_service;
        sfg_agent_msgs::srv::GetMetadata::Response m_get_agent_response;
    };
}