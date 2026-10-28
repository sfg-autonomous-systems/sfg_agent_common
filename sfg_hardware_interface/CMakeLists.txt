cmake_minimum_required(VERSION 3.8)
project(sfg_hardware_interface)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ########################################################################################################
# Find dependencies.                                                                                     #
# ########################################################################################################
find_package(ament_cmake REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(sfg_agent_msgs REQUIRED)
find_package(sfg_utils REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(tf2_ros REQUIRED)

include(FetchContent)

FetchContent_Declare(
    vicon_datastream_sdk
    GIT_REPOSITORY https://github.com/whoenig/vicon-datastream-sdk.git
    GIT_TAG a5096f283f484acca98b434c08810cd922551701
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(vicon_datastream_sdk)
set_target_properties(ViconDataStreamSDK_CPP PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ########################################################################################################
# Add nodes and libraries.                                                                               #
# ########################################################################################################
set(target_name "${PROJECT_NAME}")
sfg_utils__add_library("${target_name}"
    SOURCES
    "src/locomotion_controller_base.cpp"
    DEPENDENCIES
    PUBLIC
    Eigen3::Eigen
    ${geometry_msgs_TARGETS}
    ${nav_msgs_TARGETS}
    rclcpp::rclcpp
    ${sfg_agent_msgs_TARGETS}
    tf2_ros::tf2_ros
    PRIVATE
    sfg_utils::sfg_utils
    EXPORT_DEPENDENCIES
    Eigen3
    geometry_msgs
    nav_msgs
    rclcpp
    sfg_agent_msgs
    tf2_ros
)

sfg_utils__add_component("vicon_localizer" "${PROJECT_NAME}::ViconLocalizer"
    SOURCES
    "src/vicon_localizer_component.cpp"
    "src/vicon_localizer.cpp"
    DEPENDENCIES
    PRIVATE
    ${geometry_msgs_TARGETS}
    rclcpp_components::component
    sfg_utils::sfg_utils
    tf2_geometry_msgs::tf2_geometry_msgs
    ViconDataStreamSDK_CPP
    PUBLIC
    rclcpp::rclcpp
    tf2_ros::tf2_ros
    EXPORT_DEPENDENCIES
    rclcpp
    sfg_utils
    tf2_geometry_msgs
    tf2_ros
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