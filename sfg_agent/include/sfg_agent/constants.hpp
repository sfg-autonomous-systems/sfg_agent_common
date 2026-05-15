#pragma once

#include <cctype>

namespace sfg_agent::constants
{
    // How often the agent should send a heartbeat in seconds during the initial bootup phase.
    constexpr auto bootup_heartbeat_interval = 1;
    // How many bootup heartbeats to send before transitioning to the steady state interval.
    constexpr auto bootup_heartbeat_count = 5;
    // The standard interval once the bootup phase is over.
    constexpr auto steady_state_heartbeat_interval = 5;

}