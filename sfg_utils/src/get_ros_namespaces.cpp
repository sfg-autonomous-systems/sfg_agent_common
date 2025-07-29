#include "sfg_utils/get_ros_namespaces.hpp"

#include <tuple>

#include "sfg_utils/get_agent_name.hpp"
#include "sfg_utils/sanitize_agent_name.hpp"

namespace sfg_utils
{
    std::tuple<std::string, std::string> get_ros_namespaces()
    {
        return std::make_tuple("/local", "/global/" + sanitize_agent_name(get_agent_name()));
    }
}
