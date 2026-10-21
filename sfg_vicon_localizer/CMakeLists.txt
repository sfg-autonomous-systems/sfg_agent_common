cmake_minimum_required(VERSION 3.14)
project(sfg_vicon_localizer)

include(FetchContent)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(sfg_utils REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(tf2_ros REQUIRED)

FetchContent_Declare(
  vicon_datastream_sdk
  GIT_REPOSITORY https://github.com/whoenig/vicon-datastream-sdk.git
  GIT_TAG a5096f283f484acca98b434c08810cd922551701
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(vicon_datastream_sdk)
set_target_properties(ViconDataStreamSDK_CPP PROPERTIES POSITION_INDEPENDENT_CODE ON)

sfg_utils__add_component("localizer" "${PROJECT_NAME}::Localizer"
  SOURCES
  "src/localizer_component.cpp"
  "src/localizer.cpp"
  DEPENDENCIES
  PRIVATE
  rclcpp_components::component
  sfg_utils::sfg_utils
  tf2_geometry_msgs::tf2_geometry_msgs
  ViconDataStreamSDK_CPP
  PUBLIC
  rclcpp::rclcpp
  tf2_ros::tf2_ros
  EXPORT_DEPENDENCIES
  rclcpp
  tf2_ros
  tf2_geometry_msgs
  sfg_utils
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
