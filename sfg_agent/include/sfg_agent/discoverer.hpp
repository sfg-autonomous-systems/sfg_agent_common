#pragma once

#include <rclcpp/rclcpp.hpp>

#include "sfg_agent_msgs/msg/discovery_event.hpp"
#include "sfg_agent_msgs/msg/heartbeat.hpp"
#include "sfg_agent_msgs/srv/get_metadata.hpp"
#include "sfg_agent_msgs/srv/get_discovered_agents.hpp"

namespace sfg_agent
{
    class Discoverer : public rclcpp::Node
    {
    public:
        Discoverer(const rclcpp::NodeOptions &options);

    private:
        struct DiscoveredAgent
        {
        public:
            rclcpp::TimerBase::SharedPtr m_keepalive_timer;
            sfg_agent_msgs::msg::Metadata m_metadata;
        };

        void heartbeat_callback(const sfg_agent_msgs::msg::Heartbeat::SharedPtr msg);
        void keepalive_callback(const std::string &agent_name);
        void get_metadata_callback(
            const std::string &agent_name,
            rclcpp::Client<sfg_agent_msgs::srv::GetMetadata>::SharedFuture future);
        void get_discovered_agents_callback(
            const std::shared_ptr<sfg_agent_msgs::srv::GetDiscoveredAgents::Request> request,
            std::shared_ptr<sfg_agent_msgs::srv::GetDiscoveredAgents::Response> response);

        // ROS parameters
        uint8_t m_keepalive;
        bool m_exclude_self;

        std::string m_agent_name;
        std::map<std::string, rclcpp::Client<sfg_agent_msgs::srv::GetMetadata>::SharedPtr> m_pending_get_metadata_requests;
        std::map<std::string, std::shared_ptr<DiscoveredAgent>> m_discovered_agents;

        rclcpp::Subscription<sfg_agent_msgs::msg::Heartbeat>::SharedPtr m_heartbeat_subscriber;
        rclcpp::Publisher<sfg_agent_msgs::msg::DiscoveryEvent>::SharedPtr m_agent_discovery_event_publisher;
        rclcpp::Service<sfg_agent_msgs::srv::GetDiscoveredAgents>::SharedPtr m_get_discovered_agents_service;
    };
}