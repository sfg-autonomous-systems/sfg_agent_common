#include "sfg_utils/sanitize_hostname.hpp"

#include <cctype>
#include <regex>

namespace sfg_utils
{
    std::string sanitize_hostname(const std::string &hostname)
    {
        auto sanitized_hostname = std::string(hostname);

        // Convert to lowercase.
        std::transform(sanitized_hostname.begin(), sanitized_hostname.end(), sanitized_hostname.begin(),
                       [](char c)
                       { return std::tolower(c); });

        // Ensure remaining characters are alphanumeric or underscores.
        std::transform(sanitized_hostname.begin(), sanitized_hostname.end(), sanitized_hostname.begin(),
                       [](char c)
                       { return std::isalnum(c) ? c : '_'; });

        // Ensure hostname does not contain consequtive underscores.
        sanitized_hostname = std::regex_replace(sanitized_hostname, std::regex("_+"), "_");

        // Ensure the first character is not an underscore.
        if (sanitized_hostname.front() == '_')
        {
            sanitized_hostname.erase(sanitized_hostname.begin());
        }

        // Ensure the last character is not an underscore.
        if (sanitized_hostname.back() == '_')
        {
            sanitized_hostname.pop_back();
        }

        // If the hostname is empty, set it to "host_<random_unique_integer>".
        if (sanitized_hostname.empty())
        {
            throw std::runtime_error("Hostname cannot be made compatible with ROS topic naming convention. Please change the hostname.");
        }

        return sanitized_hostname;
    }
}