#include "sfg_composition_interfaces/lazy_composable_node_loader.hpp"

namespace sfg_composition_interfaces
{
    LazyComposableNodeLoader::LazyComposableNodeLoader(
        std::string output_topic,
        std::shared_ptr<composition_interfaces::srv::LoadNode::Request> load_request,
        rclcpp::Client<composition_interfaces::srv::LoadNode>::SharedPtr load_node_client,
        rclcpp::Client<composition_interfaces::srv::UnloadNode>::SharedPtr unload_node_client,
        rclcpp::Logger logger)
        : m_output_topic(std::move(output_topic)),
          m_load_request(std::move(load_request)),
          m_load_node_client(load_node_client),
          m_unload_node_client(unload_node_client),
          m_logger(std::move(logger))
    {
        RCLCPP_INFO(m_logger, "Created lazy composable node loader for topic '%s'. Upon request, will load node '%s' from package '%s'.", m_output_topic.c_str(), m_load_request->plugin_name.c_str(), m_load_request->package_name.c_str());
    }

    LazyComposableNodeLoader::~LazyComposableNodeLoader()
    {
        RCLCPP_INFO(m_logger, "Deleting lazy composable node loader for topic '%s'.", m_output_topic.c_str());
    }

    const std::string &LazyComposableNodeLoader::get_output_topic() const
    {
        return m_output_topic;
    }

    void LazyComposableNodeLoader::load()
    {
        std::lock_guard lock(m_mutex);
        m_desired_state = State::Loaded;

        if (m_state == State::Unloaded)
        {
            initiate_load(create_callback_context());
        }
    }

    void LazyComposableNodeLoader::unload()
    {
        std::lock_guard lock(m_mutex);
        m_desired_state = State::Unloaded;

        if (m_state == State::Loaded)
        {
            initiate_unload(m_id, create_callback_context());
        }
    }

    void LazyComposableNodeLoader::initiate_load(const CallbackContext &context)
    {
        if (auto shared_this = context.m_weak_this.lock())
        {
            std::lock_guard lock(shared_this->m_mutex);
            RCLCPP_INFO(context.m_logger, "Loading composable node for topic '%s'.", context.m_output_topic.c_str());

            shared_this->m_state = State::Loading;
            context.m_load_node_client->async_send_request(
                shared_this->m_load_request,
                [context](rclcpp::Client<composition_interfaces::srv::LoadNode>::SharedFuture future)
                {
                    load_callback(context, future);
                });
        }
    }

    void LazyComposableNodeLoader::initiate_unload(std::uint64_t id, const CallbackContext &context)
    {
        RCLCPP_INFO(context.m_logger, "Unloading composable node for topic '%s'.", context.m_output_topic.c_str());

        if (auto shared_this = context.m_weak_this.lock())
        {
            std::lock_guard lock(shared_this->m_mutex);
            shared_this->m_state = State::Unloading;
        }

        auto request = std::make_shared<composition_interfaces::srv::UnloadNode::Request>();
        request->unique_id = id;
        context.m_unload_node_client->async_send_request(
            request,
            [id, context](rclcpp::Client<composition_interfaces::srv::UnloadNode>::SharedFuture future)
            {
                unload_callback(id, context, future);
            });
    }

    void LazyComposableNodeLoader::load_callback(const CallbackContext &context, rclcpp::Client<composition_interfaces::srv::LoadNode>::SharedFuture future)
    {
        // ToDo: Perhaps we should think of implementing some sort of retry logic if loading of the node failed.
        if (!future.valid())
        {
            RCLCPP_ERROR(context.m_logger, "Failed to load composable node for topic '%s'.", context.m_output_topic.c_str());

            if (auto shared_this = context.m_weak_this.lock())
            {
                std::lock_guard lock(shared_this->m_mutex);
                shared_this->m_state = State::Unloaded;
            }
            return;
        }

        auto response = future.get();

        if (!response->success)
        {
            RCLCPP_ERROR(context.m_logger, "Failed to load composable node for topic '%s'.", context.m_output_topic.c_str());

            if (auto shared_this = context.m_weak_this.lock())
            {
                std::lock_guard lock(shared_this->m_mutex);
                shared_this->m_state = State::Unloaded;
            }
            return;
        }

        auto id = response->unique_id;
        auto shared_this = context.m_weak_this.lock();

        if (!shared_this)
        {
            // We have been deleted since the service to load the node was called.
            initiate_unload(id, context);
            RCLCPP_WARN(context.m_logger, "Loaded composable node for topic '%s' with ID '%lu' but it is no longer required. Unloading node again.", context.m_output_topic.c_str(), id);
            return;
        }

        std::lock_guard lock(shared_this->m_mutex);

        if (shared_this->m_desired_state == State::Unloaded)
        {
            // Someone has requested to unload the node again since the service to load the node was called.
            initiate_unload(id, context);
            RCLCPP_WARN(context.m_logger, "Loaded composable node for topic '%s' with ID '%lu' but it is no longer required. Unloading node again.", context.m_output_topic.c_str(), id);
            return;
        }

        shared_this->m_id = id;
        shared_this->m_state = State::Loaded;
        RCLCPP_INFO(context.m_logger, "Loaded composable node for topic '%s' with ID '%lu'.", context.m_output_topic.c_str(), id);
    }

    void LazyComposableNodeLoader::unload_callback(std::uint64_t id, const CallbackContext &context, rclcpp::Client<composition_interfaces::srv::UnloadNode>::SharedFuture future)
    {
        // ToDo: Perhaps we should implement some sort of retry logic if unloading of the node failed.
        if (!future.valid())
        {
            RCLCPP_ERROR(context.m_logger, "Failed to unload composable node for topic '%s' with ID '%lu': Future is invalid.", context.m_output_topic.c_str(), id);

            if (auto shared_this = context.m_weak_this.lock())
            {
                std::lock_guard lock(shared_this->m_mutex);
                shared_this->m_state = State::Loaded;
            }
            return;
        }

        auto response = future.get();

        if (!response->success)
        {
            RCLCPP_ERROR(context.m_logger, "Failed to unload composable node for topic '%s' with ID '%lu': %s", context.m_output_topic.c_str(), id, response->error_message.c_str());

            if (auto shared_this = context.m_weak_this.lock())
            {
                std::lock_guard lock(shared_this->m_mutex);
                shared_this->m_state = State::Loaded;
            }
            return;
        }

        if (auto shared_this = context.m_weak_this.lock())
        {
            std::lock_guard lock(shared_this->m_mutex);
            shared_this->m_state = State::Unloaded;

            if (shared_this->m_desired_state == State::Loaded)
            {
                // Someone has requested to load the node again since the service to unload the node was called.
                initiate_load(context);
                RCLCPP_WARN(context.m_logger, "Unloaded composable node for topic '%s' with ID '%lu' but it is still required. Loading node again.", context.m_output_topic.c_str(), id);
            }
        }
        RCLCPP_INFO(context.m_logger, "Unloaded composable node for topic '%s' with ID '%lu'.", context.m_output_topic.c_str(), id);
    }

    LazyComposableNodeLoader::CallbackContext LazyComposableNodeLoader::create_callback_context()
    {
        return CallbackContext{weak_from_this(), m_load_node_client, m_unload_node_client, m_logger, m_output_topic};
    }
}