#include "sfg_utils/get_hostname.hpp"

#include <cstdlib>
#include <cstring>
#include <limits.h>
#include <stdexcept>
#include <unistd.h>

namespace sfg_utils
{
    std::string get_hostname()
    {
        const char *env_hostname = std::getenv("AGENT_HOSTNAME");

        if (env_hostname != nullptr && std::strlen(env_hostname) > 0)
        {
            return std::string(env_hostname);
        }

        char hostname[HOST_NAME_MAX + 1];

        if (gethostname(hostname, sizeof(hostname)) != 0)
        {
            throw std::runtime_error("Failed to get hostname: " + std::string(strerror(errno)));
        }

        return hostname;
    }
}
