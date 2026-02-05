#include "sfg_pluginlib/plugin_loader.hpp"

namespace sfg_pluginlib
{
    template <typename PluginType>
    PluginLoader<PluginType>::Instance::Instance(std::shared_ptr<ClassLoaderType> class_loader)
        : m_class_loader(class_loader),
          m_lease_count(0),
          m_timer_version(0)
    {
    }

    template <typename PluginType>
    std::unordered_map<std::string, typename PluginLoader<PluginType>::Instance> PluginLoader<PluginType>::s_instances;
    template <typename PluginType>
    std::mutex PluginLoader<PluginType>::s_mutex;

    template <typename PluginType>
    std::shared_ptr<typename PluginLoader<PluginType>::ClassLoaderType> PluginLoader<PluginType>::get(std::string package, std::string base_class)
    {
        std::lock_guard lock(s_mutex);
        auto key = compute_key(package, base_class);
        auto iterator = s_instances.find(key);

        if (iterator == s_instances.end())
        {
            iterator = s_instances.emplace(
                                      std::piecewise_construct,
                                      std::forward_as_tuple(key),
                                      std::forward_as_tuple(std::make_shared<ClassLoaderType>(package, base_class)))
                           .first;
        }

        auto &instance = iterator->second;
        instance.m_lease_count++;
        instance.m_timer_version++;

        auto lease_guard = std::shared_ptr<void>(
            nullptr,
            [package, base_class](void *)
            {
                PluginLoader<PluginType>::release(package, base_class);
            });

        return std::shared_ptr<ClassLoaderType>(lease_guard, instance.m_class_loader.get());
    }

    template <typename PluginType>
    void PluginLoader<PluginType>::release(std::string package, std::string base_class)
    {
        std::lock_guard lock(s_mutex);
        auto key = compute_key(package, base_class);
        auto iterator = s_instances.find(key);

        if (iterator == s_instances.end())
        {
            return;
        }

        auto &instance = iterator->second;

        if (--instance.m_lease_count != 0)
        {
            return;
        }

        auto timer_version = instance.m_timer_version;

        std::thread(
            [key, timer_version]()
            {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                std::lock_guard<std::mutex> lock(s_mutex);
                auto iterator = s_instances.find(key);

                if (iterator == s_instances.end())
                {
                    return;
                }

                auto &instance = iterator->second;

                if (instance.m_lease_count == 0 && timer_version == instance.m_timer_version)
                {
                    s_instances.erase(iterator);
                }
            })
            .detach();
    }

    template <typename PluginType>
    std::string PluginLoader<PluginType>::compute_key(std::string package, std::string base_class)
    {
        return package + "|" + base_class;
    }
}