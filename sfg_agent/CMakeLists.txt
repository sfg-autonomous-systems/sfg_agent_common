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
find_package(sfg_composition_interfaces REQUIRED)
find_package(sfg_utils REQUIRED)
find_package(yaml-cpp REQUIRED)

# ########################################################################################################
# Add nodes and libraries.                                                                               #
# ########################################################################################################
sfg_utils__add_component("agent_status_provider" "${PROJECT_NAME}::AgentStatusProvider"
  SOURCES
  "src/agent_status_provider_component.cpp"
  "src/agent_status_provider.cpp"
  AMENT_DEPENDENCIES
  rclcpp
  rclcpp_components
  sfg_agent_msgs
  sfg_utils
  SYSTEM_DEPENDENCIES
  yaml-cpp
)

sfg_utils__add_component("agent_discoverer" "${PROJECT_NAME}::AgentDiscoverer"
  SOURCES
  "src/agent_discoverer_component.cpp"
  "src/agent_discoverer.cpp"
  AMENT_DEPENDENCIES
  rclcpp
  rclcpp_components
  sfg_agent_msgs
  sfg_utils
)

sfg_utils__add_component("agent_decoder" "${PROJECT_NAME}::AgentDecoder"
  SOURCES
  "src/agent_decoder_component.cpp"
  "src/agent_decoder.cpp"
  AMENT_DEPENDENCIES
  rclcpp
  rclcpp_components
  sfg_agent_msgs
  sfg_composition_interfaces
  sfg_utils
)

# ########################################################################################################
# Add other files/directories.                                                                           #
# ########################################################################################################
install(DIRECTORY
  "launch"
  DESTINATION "share/${PROJECT_NAME}/"
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