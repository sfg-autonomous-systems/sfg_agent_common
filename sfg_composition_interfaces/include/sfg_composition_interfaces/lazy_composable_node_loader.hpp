#pragma once

#include <composition_interfaces/srv/load_node.hpp>
#include <composition_interfaces/srv/unload_node.hpp>
#include <rclcpp/rclcpp.hpp>

namespace sfg_composition_interfaces
{
    class LazyComposableNodeLoader : public std::enable_shared_from_this<LazyComposableNodeLoader>
    {
    public:
        enum class State
        {
            Loading,
            Loaded,
            Unloaded,
            Unloading
        };

        LazyComposableNodeLoader(
            std::shared_ptr<composition_interfaces::srv::LoadNode::Request> load_request,
            rclcpp::Client<composition_interfaces::srv::LoadNode>::SharedPtr load_node_client,
            rclcpp::Client<composition_interfaces::srv::UnloadNode>::SharedPtr unload_node_client,
            rclcpp::Logger logger);

        void load();
        void unload();

    private:
        struct CallbackContext
        {
        public:
            std::weak_ptr<LazyComposableNodeLoader> m_weak_this;
            rclcpp::Client<composition_interfaces::srv::LoadNode>::SharedPtr m_load_node_client;
            rclcpp::Client<composition_interfaces::srv::UnloadNode>::SharedPtr m_unload_node_client;
            rclcpp::Logger m_logger;

            // For logging purposes only.
            std::string m_plugin_name;
            std::string m_package_name;
        };

        static void initiate_load(const CallbackContext &context);
        static void initiate_unload(std::uint64_t id, const CallbackContext &context);
        static void load_callback(const CallbackContext &context, rclcpp::Client<composition_interfaces::srv::LoadNode>::SharedFuture future);
        static void unload_callback(std::uint64_t id, const CallbackContext &context, rclcpp::Client<composition_interfaces::srv::UnloadNode>::SharedFuture future);

        CallbackContext create_callback_context();

        State m_state = State::Unloaded;
        State m_desired_state = State::Unloaded;
        std::uint64_t m_id = 0;
        std::recursive_mutex m_mutex;

        const std::shared_ptr<composition_interfaces::srv::LoadNode::Request> m_load_request;
        rclcpp::Client<composition_interfaces::srv::LoadNode>::SharedPtr m_load_node_client;
        rclcpp::Client<composition_interfaces::srv::UnloadNode>::SharedPtr m_unload_node_client;
        rclcpp::Logger m_logger;
    };
}