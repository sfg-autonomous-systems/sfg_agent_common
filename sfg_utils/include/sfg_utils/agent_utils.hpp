#pragma once

#include <string>

namespace sfg_utils
{
    std::string get_agent_name();
    std::string sanitize_agent_name(const std::string &agent_name);
}