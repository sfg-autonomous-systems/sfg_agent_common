#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "sfg_utils/get_hostname.hpp"
#include "sfg_utils/sanitize_hostname.hpp"

namespace py = pybind11;

PYBIND11_MODULE(sfg_utils_py, module)
{
    module.def("get_hostname", &sfg_utils::get_hostname);
    module.def("sanitize_hostname", &sfg_utils::sanitize_hostname);
}