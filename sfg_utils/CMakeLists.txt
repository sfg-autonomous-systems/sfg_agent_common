cmake_minimum_required(VERSION 3.8)
project(sfg_utils)

include("cmake/add_library.cmake")
include("cmake/generate_python_stubs.cmake")

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ########################################################################################################
# Find dependencies.                                                                                     #
# ########################################################################################################
find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 3.0 REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rmw REQUIRED)

# ########################################################################################################
# Add nodes and libraries.                                                                               #
# ########################################################################################################
set(target_name_cpp "${PROJECT_NAME}")
sfg_utils__add_library("${target_name_cpp}"
  SOURCES
  "src/fqn/ros_fqn_builder.cpp"
  "src/agent_utils.cpp"
  "src/ros_utils.cpp"
  "src/cpp_utils.cpp"
  DEPENDENCIES
  rclcpp::rclcpp
  rmw::rmw
  magic_enum::magic_enum
  EXPORT_DEPENDENCIES
  magic_enum
)

set(target_name_py "${PROJECT_NAME}_py")
pybind11_add_module("${target_name_py}" MODULE "src/python_bindings.cpp")
target_link_libraries("${target_name_py}" PRIVATE
  "${target_name_cpp}"
)
ament_python_install_package(${PROJECT_NAME})
install(
  TARGETS "${target_name_py}"
  DESTINATION "${PYTHON_INSTALL_DIR}/${PROJECT_NAME}"
)
generate_python_stubs("${target_name_py}"
  OUTPUT_DIRECTORY "${PROJECT_NAME}"
  STUBGEN_ARGS "--enum-class-locations" "RosFqnSegment:${target_name_py}.fqn"
)

install(PROGRAMS
  "sfg_utils/onshape/export.py"
  DESTINATION "lib/${PROJECT_NAME}"
  RENAME "onshape_export"
)

# ########################################################################################################
# Add other files/directories.                                                                           #
# ########################################################################################################
install(DIRECTORY
  "cmake"
  DESTINATION "share/${PROJECT_NAME}/"
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  find_package(ament_cmake_gtest REQUIRED)

  # The following line skips the linter which checks for copyrights.
  # Comment the line when a copyright and license is added to all source files.
  set(ament_cmake_copyright_FOUND TRUE)

  set(tests_target_name "${PROJECT_NAME}_tests")
  ament_add_gtest("${tests_target_name}" "test/test_ros_fqn_builder.cpp")
  target_include_directories("${tests_target_name}" PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  )
  target_link_libraries("${tests_target_name}" "${target_name_cpp}")

  # The following line skips cpplint (only works in a git repo).
  # Comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files.
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package(CONFIG_EXTRAS "cmake/common.cmake")