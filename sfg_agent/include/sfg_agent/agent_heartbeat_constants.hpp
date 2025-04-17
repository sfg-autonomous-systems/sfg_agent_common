#pragma once

#include <cctype>

namespace sfg_agent
{
    // How often the agent should send a heartbeat in seconds.
    // This value is intentionally hard-coded to ensure it is the same
    // across all agents.
    constexpr uint8_t AGENT_HEARTBEAT_INTERVAL = 5;
}