cmake_minimum_required(VERSION 3.8)
project(sfg_agent)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ########################################################################################################
# Find dependencies.                                                                                     #
# ########################################################################################################
find_package(ament_cmake REQUIRED)
find_package(isaac_ros_h264_decoder REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sfg_agent_msgs REQUIRED)
find_package(sfg_utils REQUIRED)
find_package(yaml-cpp REQUIRED)

# ########################################################################################################
# Add nodes and libraries.                                                                               #
# ########################################################################################################
set(target "agent_status_provider")
add_executable("${target}" "src/agent_status_provider.cpp" "src/agent_status_provider_node.cpp")
target_include_directories("${target}" PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features("${target}" PUBLIC c_std_99 cxx_std_17)
ament_target_dependencies("${target}"
  rclcpp
  sfg_agent_msgs
  sfg_utils
)

# For some reason ament_target_dependencies does not work for yaml-cpp.
target_link_libraries("${target}"
  yaml-cpp
)
install(TARGETS
  ${target}
  DESTINATION "lib/${PROJECT_NAME}"
)

set(target "agent_decoder")
add_executable("${target}" "src/agent_decoder.cpp" "src/agent_decoder_node.cpp")
target_include_directories("${target}" PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features("${target}" PUBLIC c_std_99 cxx_std_17)
ament_target_dependencies("${target}"
  isaac_ros_h264_decoder
  rclcpp
  sfg_agent_msgs
  sfg_utils
)
install(TARGETS
  ${target}
  DESTINATION "lib/${PROJECT_NAME}"
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