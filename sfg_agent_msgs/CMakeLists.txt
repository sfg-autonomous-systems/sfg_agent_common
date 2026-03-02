cmake_minimum_required(VERSION 3.8)
project(sfg_agent_msgs)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ########################################################################################################
# Find dependencies.                                                                                     #
# ########################################################################################################
find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)

# ########################################################################################################
# Add interfaces.                                                                                        #
# ########################################################################################################
set(action_files
  "action/FollowTrajectory.action"
)
set(msg_files
  "msg/DiscoveryEvent.msg"
  "msg/Heartbeat.msg"
  "msg/Metadata.msg"
  "msg/Trajectory.msg"
  "msg/Waypoint.msg"
)
set(srv_files
  "srv/GetDiscoveredAgents.srv"
  "srv/GetMetadata.srv"
  "srv/TriggerAction.srv"
)
rosidl_generate_interfaces(${PROJECT_NAME}
  ${action_files}
  ${msg_files}
  ${srv_files}
  DEPENDENCIES
  geometry_msgs
  std_msgs
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