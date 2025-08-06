#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "sfg_utils/agent_utils.hpp"
#include "sfg_utils/fqn/ros_fqn_builder.hpp"
#include "sfg_utils/ros_utils.hpp"

namespace py = pybind11;
using namespace pybind11::literals;

PYBIND11_MODULE(sfg_utils_py, module)
{
    module.def("get_agent_name", &sfg_utils::get_agent_name);
    module.def("sanitize_agent_name", &sfg_utils::sanitize_agent_name);

    py::module_ fqn_submodule = module.def_submodule("fqn");

    py::enum_<sfg_utils::fqn::Scope>(fqn_submodule, "Scope")
        .value("Global", sfg_utils::fqn::Scope::Global)
        .value("Local", sfg_utils::fqn::Scope::Local);

    py::enum_<sfg_utils::fqn::Component>(fqn_submodule, "Component")
        .value("Camera", sfg_utils::fqn::Component::Camera)
        .value("Lidar", sfg_utils::fqn::Component::Lidar)
        .value("IMU", sfg_utils::fqn::Component::IMU)
        .value("Custom", sfg_utils::fqn::Component::Custom);

    py::enum_<sfg_utils::fqn::Endpoint>(fqn_submodule, "Endpoint")
        .value("ColorImageRaw", sfg_utils::fqn::Endpoint::ColorImageRaw)
        .value("DepthImageRaw", sfg_utils::fqn::Endpoint::DepthImageRaw)
        .value("ColorImageCompressed", sfg_utils::fqn::Endpoint::ColorImageCompressed)
        .value("DepthImageCompressed", sfg_utils::fqn::Endpoint::DepthImageCompressed)
        .value("CameraColorInfo", sfg_utils::fqn::Endpoint::CameraColorInfo)
        .value("CameraDepthInfo", sfg_utils::fqn::Endpoint::CameraDepthInfo)
        .value("PointCloud", sfg_utils::fqn::Endpoint::PointCloud)
        .value("IMU", sfg_utils::fqn::Endpoint::IMU)
        .value("Custom", sfg_utils::fqn::Endpoint::Custom);

    py::class_<sfg_utils::fqn::RosFQNBuilder>(fqn_submodule, "RosFQNBuilder")
        .def(py::init<>())
        .def("scope", &sfg_utils::fqn::RosFQNBuilder::scope, "scope"_a)
        .def("agent", &sfg_utils::fqn::RosFQNBuilder::agent, "name"_a = "")
        .def("component", &sfg_utils::fqn::RosFQNBuilder::component, "type"_a, "identifier"_a)
        .def("endpoint", &sfg_utils::fqn::RosFQNBuilder::endpoint, "type"_a, "stream"_a = "", "resource"_a = "")
        .def("build", &sfg_utils::fqn::RosFQNBuilder::build, "only_namespace"_a = false);
}