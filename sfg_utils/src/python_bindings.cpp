#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "sfg_utils/get_agent_name.hpp"
#include "sfg_utils/sanitize_agent_name.hpp"

namespace py = pybind11;

PYBIND11_MODULE(sfg_utils_py, module)
{
    module.def("get_agent_name", &sfg_utils::get_agent_name);
    module.def("sanitize_agent_name", &sfg_utils::sanitize_agent_name);
}