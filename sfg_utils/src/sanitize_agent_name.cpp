#include "sfg_utils/sanitize_agent_name.hpp"

#include <cctype>
#include <regex>

namespace sfg_utils
{
    std::string sanitize_agent_name(const std::string &agent_name)
    {
        auto sanitized_agent_name = agent_name;

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
            throw std::runtime_error("Agent name '" + agent_name + "' cannot be made compatible with ROS topic naming convention. Please change the agent name.");
        }

        return sanitized_agent_name;
    }
}