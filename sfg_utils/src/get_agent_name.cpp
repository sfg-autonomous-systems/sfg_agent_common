#include "sfg_utils/get_agent_name.hpp"

#include <cstdlib>
#include <cstring>
#include <limits.h>
#include <stdexcept>
#include <unistd.h>

namespace sfg_utils
{
    std::string get_agent_name()
    {
        const char *agent_name = std::getenv("AGENT_NAME");

        if (agent_name != nullptr && std::strlen(agent_name) > 0)
        {
            return std::string(agent_name);
        }

        char hostname[HOST_NAME_MAX + 1];

        if (gethostname(hostname, sizeof(hostname)) != 0)
        {
            throw std::runtime_error("Failed to get hostname: " + std::string(strerror(errno)));
        }

        return hostname;
    }
}
