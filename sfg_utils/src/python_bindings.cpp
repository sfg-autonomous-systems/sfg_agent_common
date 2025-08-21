#include <magic_enum.hpp>
#include <pybind11/native_enum.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "sfg_utils/agent_utils.hpp"
#include "sfg_utils/fqn/ros_fqn_builder.hpp"
#include "sfg_utils/ros_utils.hpp"

namespace py = pybind11;
using namespace pybind11::literals;

namespace
{
    template <typename TEnum>
    void bind_enum(py::module module, const char *enum_type = "enum.Enum")
    {
        auto py_enum = py::native_enum<TEnum>(module, magic_enum::enum_type_name<TEnum>().data(), enum_type);

        for (const auto &[value, name] : magic_enum::enum_entries<TEnum>())
        {
            py_enum.value(name.data(), value);
        }

        py_enum.finalize();
    }
}

PYBIND11_MODULE(sfg_utils_py, module)
{
    module.def("get_agent_name", &sfg_utils::get_agent_name);
    module.def("sanitize_agent_name", &sfg_utils::sanitize_agent_name, "name"_a);

    py::module_ fqn_submodule = module.def_submodule("fqn");

    bind_enum<sfg_utils::fqn::RosFqnSegment>(fqn_submodule, "enum.IntFlag");
    bind_enum<sfg_utils::fqn::Scope>(fqn_submodule);
    bind_enum<sfg_utils::fqn::Component>(fqn_submodule);
    bind_enum<sfg_utils::fqn::Stream>(fqn_submodule);
    bind_enum<sfg_utils::fqn::Resource>(fqn_submodule, "enum.IntFlag");

    py::class_<sfg_utils::fqn::RosFqnBuilder>(fqn_submodule, "RosFqnBuilder")
        .def(py::init<>())
        .def("scope", &sfg_utils::fqn::RosFqnBuilder::scope, "scope"_a)
        .def("agent", &sfg_utils::fqn::RosFqnBuilder::agent, "name"_a = "")
        .def("component", &sfg_utils::fqn::RosFqnBuilder::component, "component"_a, "name"_a = "")
        .def("stream", &sfg_utils::fqn::RosFqnBuilder::stream, "stream"_a, "name"_a = "")
        .def("resource", &sfg_utils::fqn::RosFqnBuilder::resource, "resource"_a, "name"_a = "")
        .def("build", py::overload_cast<sfg_utils::fqn::RosFqnSegment, sfg_utils::fqn::RosFqnSegment>(&sfg_utils::fqn::RosFqnBuilder::build, py::const_), "begin"_a, "end"_a)
        .def("build", py::overload_cast<sfg_utils::fqn::RosFqnSegment>(&sfg_utils::fqn::RosFqnBuilder::build, py::const_), "segment"_a)
        .def("build", py::overload_cast<>(&sfg_utils::fqn::RosFqnBuilder::build, py::const_))
        .def("reset", py::overload_cast<>(&sfg_utils::fqn::RosFqnBuilder::reset))
        .def("reset", py::overload_cast<sfg_utils::fqn::RosFqnSegment>(&sfg_utils::fqn::RosFqnBuilder::reset), "segments"_a);
}