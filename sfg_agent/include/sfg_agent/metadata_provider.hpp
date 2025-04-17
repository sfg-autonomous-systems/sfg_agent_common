#pragma once

#include <cstdint>
#include <filesystem>
#include <rclcpp/rclcpp.hpp>

#include "sfg_agent_msgs/srv/get_metadata.hpp"

namespace sfg_agent
{
    class MetadataProvider : public rclcpp::Node
    {
    public:
        MetadataProvider();

    private:
        void get_metadata(const std::shared_ptr<sfg_agent_msgs::srv::GetMetadata::Request> request,
                          std::shared_ptr<sfg_agent_msgs::srv::GetMetadata::Response> response);
        bool load_metadata(const std::filesystem::path &filepath);

        // ROS parameters
        std::string m_metadata_filepath;

        sfg_agent_msgs::srv::GetMetadata::Response m_metadata_response;
        rclcpp::Service<sfg_agent_msgs::srv::GetMetadata>::SharedPtr m_service;
    };
}