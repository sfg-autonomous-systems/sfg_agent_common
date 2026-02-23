#include "sfg_utils/agent_utils.hpp"

#include <cstdlib>
#include <cstring>
#include <limits.h>
#include <regex>
#include <stdexcept>
#include <unistd.h>

namespace sfg_utils::agent_utils
{
    std::string get_agent_name()
    {
        const char *name = std::getenv("SFG_AGENT_NAME");

        if (name != nullptr && std::strlen(name) > 0)
        {
            return std::string(name);
        }

        char hostname[HOST_NAME_MAX + 1];

        if (gethostname(hostname, sizeof(hostname)) != 0)
        {
            throw std::runtime_error("Failed to get hostname: " + std::string(strerror(errno)));
        }

        return hostname;
    }

    std::string sanitize_agent_name(const std::string &name)
    {
        auto sanitized_agent_name = name;

        // Convert to lowercase.
        std::transform(sanitized_agent_name.begin(), sanitized_agent_name.end(), sanitized_agent_name.begin(),
                       [](char c)
                       { return std::tolower(c); });

        // Ensure remaining characters are alphanumeric or underscores.
        std::transform(sanitized_agent_name.begin(), sanitized_agent_name.end(), sanitized_agent_name.begin(),
                       [](char c)
                       { return std::isalnum(c) ? c : '_'; });

        // Ensure agent name does not contain consequtive underscores.
        sanitized_agent_name = std::regex_replace(sanitized_agent_name, std::regex("_+"), "_");

        // Ensure the first character is not an underscore.
        if (sanitized_agent_name.front() == '_')
        {
            sanitized_agent_name.erase(sanitized_agent_name.begin());
        }

        // Ensure the last character is not an underscore.
        if (sanitized_agent_name.back() == '_')
        {
            sanitized_agent_name.pop_back();
        }

        // If the agent name is empty, set it to "host_<random_unique_integer>".
        if (sanitized_agent_name.empty())
        {
            throw std::runtime_error("Agent name '" + name + "' cannot be made compatible with ROS topic naming convention. Please change the agent name.");
        }

        return sanitized_agent_name;
    }
}
