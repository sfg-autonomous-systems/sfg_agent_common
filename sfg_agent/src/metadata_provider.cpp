#include "sfg_agent/metadata_provider.hpp"

#include <yaml-cpp/yaml.h>

namespace sfg_agent
{
    MetadataProvider::MetadataProvider() : Node("metadata_provider")
    {
        RCLCPP_INFO(get_logger(), "Starting metadata provider.");

        // Declare and retrieve ROS parameters.
        declare_parameter("metadata_filepath", 0, rcl_interfaces::msg::ParameterDescriptor().set__description("The filepath pointing to the yaml file containing the metadata."));
        get_parameter("metadata_filepath", m_metadata_filepath);

        if (!load_metadata(m_metadata_filepath))
        {
            RCLCPP_ERROR(get_logger(), "Failed to load metadata from '%s'.", m_metadata_filepath.c_str());
            throw std::runtime_error("Failed to load metadata.");
        }

        // Set up interfaces.
        m_service = create_service<sfg_agent_msgs::srv::GetMetadata>(
            "get_metadata",
            std::bind(&MetadataProvider::get_metadata, this, std::placeholders::_1, std::placeholders::_2));
    }

    void MetadataProvider::get_metadata(const std::shared_ptr<sfg_agent_msgs::srv::GetMetadata::Request> request,
                                        std::shared_ptr<sfg_agent_msgs::srv::GetMetadata::Response> response)
    {
        RCLCPP_INFO(get_logger(), "Received request for metadata.");
        *response = m_metadata_response;
    }

    bool MetadataProvider::load_metadata(const std::filesystem::path &filepath)
    {
        // Load the YAML file.
        try
        {
            YAML::Node config = YAML::LoadFile(filepath.string());

            if (!config.IsMap())
            {
                RCLCPP_ERROR(get_logger(), "Metadata file '%s' is not a valid YAML map.", filepath.c_str());
                return false;
            }

            if (config["cameras"])
            {
                m_metadata_response.capabilities |= sfg_agent_msgs::srv::GetMetadata::Request::CAPABILITY_CAMERA;
                m_metadata_response.cameras = config["cameras"].as<std::vector<std::string>>();
            }

            if (config["lidars"])
            {
                m_metadata_response.capabilities |= sfg_agent_msgs::srv::GetMetadata::Request::CAPABILITY_LIDAR;
                m_metadata_response.lidars = config["lidars"].as<std::vector<std::string>>();
            }
        }
        catch (const YAML::Exception &exception)
        {
            RCLCPP_ERROR(get_logger(), "Failed to parse metadata file '%s': %s", filepath.c_str(), exception.what());
            return false;
        }

        return true;
    }
}