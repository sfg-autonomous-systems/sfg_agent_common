cmake_minimum_required(VERSION 3.8)
project(sfg_agent)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ########################################################################################################
# Find dependencies.                                                                                     #
# ########################################################################################################
find_package(ament_cmake REQUIRED)
find_package(composition_interfaces REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(sfg_agent_msgs REQUIRED)
find_package(sfg_utils REQUIRED)
find_package(yaml-cpp REQUIRED)

# ########################################################################################################
# Add nodes and libraries.                                                                               #
# ########################################################################################################
set(executable_name "agent_status_provider")
set(target_name "${executable_name}_component")
set(export_name "export_${target_name}")
add_library("${target_name}" SHARED "src/agent_status_provider.cpp" "src/agent_status_provider_component.cpp")
rclcpp_components_register_node(
  "${target_name}"
  PLUGIN "sfg_agent::AgentStatusProvider"
  EXECUTABLE "${executable_name}"
)
target_include_directories("${target_name}" PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features("${target_name}" PUBLIC c_std_99 cxx_std_17)
ament_target_dependencies("${target_name}"
  rclcpp
  rclcpp_components
  sfg_agent_msgs
  sfg_utils
)
target_link_libraries("${target_name}"
  yaml-cpp
)
ament_export_targets("${export_name}")
install(
  TARGETS "${target_name}"
  EXPORT "${export_name}"
  LIBRARY DESTINATION "lib"
  ARCHIVE DESTINATION "lib"
  RUNTIME DESTINATION "bin"
  INCLUDES DESTINATION "include"
)

set(executable_name "agent_discoverer")
set(target_name "${executable_name}_component")
set(export_name "export_${target_name}")
add_library("${target_name}" SHARED "src/agent_discoverer.cpp" "src/agent_discoverer_component.cpp")
rclcpp_components_register_node(
  "${target_name}"
  PLUGIN "sfg_agent::AgentDiscoverer"
  EXECUTABLE "${executable_name}"
)
target_include_directories("${target_name}" PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features("${target_name}" PUBLIC c_std_99 cxx_std_17)
ament_target_dependencies("${target_name}"
  rclcpp
  rclcpp_components
  sfg_agent_msgs
  sfg_utils
)
ament_export_targets("${export_name}")
install(
  TARGETS "${target_name}"
  EXPORT "${export_name}"
  LIBRARY DESTINATION "lib"
  ARCHIVE DESTINATION "lib"
  RUNTIME DESTINATION "bin"
  INCLUDES DESTINATION "include"
)

set(executable_name "agent_decoder")
set(target_name "${executable_name}_component")
set(export_name "export_${target_name}")
add_library("${target_name}" SHARED "src/agent_decoder.cpp" "src/agent_decoder_component.cpp")
rclcpp_components_register_node(
  "${target_name}"
  PLUGIN "sfg_agent::AgentDecoder"
  EXECUTABLE "${executable_name}"
)
target_include_directories("${target_name}" PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features("${target_name}" PUBLIC c_std_99 cxx_std_17)
ament_target_dependencies("${target_name}"
  composition_interfaces
  rclcpp
  rclcpp_components
  sfg_agent_msgs
  sfg_utils
)
ament_export_targets("${export_name}")
install(
  TARGETS "${target_name}"
  EXPORT "${export_name}"
  LIBRARY DESTINATION "lib"
  ARCHIVE DESTINATION "lib"
  RUNTIME DESTINATION "bin"
  INCLUDES DESTINATION "include"
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)

  # The following line skips the linter which checks for copyrights.
  # Comment the line when a copyright and license is added to all source files.
  set(ament_cmake_copyright_FOUND TRUE)

  # The following line skips cpplint (only works in a git repo).
  # Comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files.
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()