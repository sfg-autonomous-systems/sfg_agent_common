#pragma once

#include <cctype>
#include <pluginlib/class_loader.hpp>

namespace sfg_pluginlib
{
    template <typename PluginType>
    class PluginLoader
    {
    public:
        using ClassLoaderType = pluginlib::ClassLoader<PluginType>;

        static std::shared_ptr<ClassLoaderType> get(std::string package, std::string base_class);

    private:
        class Instance
        {
        public:
            Instance(std::shared_ptr<ClassLoaderType> class_loader);

            std::shared_ptr<ClassLoaderType> m_class_loader;
            std::uint32_t m_lease_count;
            std::uint64_t m_timer_version;
        };

        static void release(std::string package, std::string base_class);
        static std::string compute_key(std::string package, std::string base_class);

        static std::unordered_map<std::string, Instance> s_instances;
        static std::mutex s_mutex;
    };
}

#include "sfg_pluginlib/plugin_loader.tpp"