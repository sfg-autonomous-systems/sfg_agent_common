#pragma once

#include <cstdint>
#include <filesystem>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/timer.hpp>

#include "sfg_agent_msgs/msg/heartbeat.hpp"
#include "sfg_agent_msgs/srv/get_metadata.hpp"

namespace sfg_agent
{
    class StatusProvider : public rclcpp::Node
    {
    public:
        StatusProvider();

    private:
        void publish_heartbeat();
        void get_metadata(const std::shared_ptr<sfg_agent_msgs::srv::GetMetadata::Request> request,
                          std::shared_ptr<sfg_agent_msgs::srv::GetMetadata::Response> response);
        bool load_metadata(const std::filesystem::path &filepath);
        std::string get_sanitized_hostname();

        // ROS parameters
        std::string m_override_hostname;
        int8_t m_heartbeat_interval;
        std::string m_metadata_filepath;

        std::string m_hostname;
        rclcpp::Publisher<sfg_agent_msgs::msg::Heartbeat>::SharedPtr m_heartbeat_publisher;
        rclcpp::TimerBase::SharedPtr m_timer;
        rclcpp::Service<sfg_agent_msgs::srv::GetMetadata>::SharedPtr m_metadata_service;
        sfg_agent_msgs::srv::GetMetadata::Response m_metadata_response;
    };
}