#include <magic_enum.hpp>
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
    void bind_enum(py::module module)
    {
        auto py_enum = py::enum_<TEnum>(module, magic_enum::enum_type_name<TEnum>().data());

        for (const auto &[value, name] : magic_enum::enum_entries<TEnum>())
        {
            py_enum.value(name.data(), value);
        }
    }
}

PYBIND11_MODULE(sfg_utils_py, module)
{
    module.def("get_agent_name", &sfg_utils::get_agent_name);
    module.def("sanitize_agent_name", &sfg_utils::sanitize_agent_name, "name"_a);

    py::module_ fqn_submodule = module.def_submodule("fqn");

    bind_enum<sfg_utils::fqn::Scope>(fqn_submodule);
    bind_enum<sfg_utils::fqn::Component>(fqn_submodule);
    bind_enum<sfg_utils::fqn::Stream>(fqn_submodule);
    bind_enum<sfg_utils::fqn::Resource>(fqn_submodule);

    py::class_<sfg_utils::fqn::RosFQNBuilder>(fqn_submodule, "RosFQNBuilder")
        .def(py::init<>())
        .def("scope", &sfg_utils::fqn::RosFQNBuilder::scope, "scope"_a)
        .def("agent", &sfg_utils::fqn::RosFQNBuilder::agent, "name"_a = "")
        .def("component", &sfg_utils::fqn::RosFQNBuilder::component, "component"_a, "name"_a)
        .def("stream", &sfg_utils::fqn::RosFQNBuilder::stream, "stream"_a, "name"_a = "")
        .def("resource", &sfg_utils::fqn::RosFQNBuilder::resource, "resource"_a, "name"_a = "")
        .def("build", &sfg_utils::fqn::RosFQNBuilder::build, "only_namespace"_a = false);
}