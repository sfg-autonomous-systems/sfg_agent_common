cmake_minimum_required(VERSION 3.8)
project(sfg_utils)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ########################################################################################################
# Find dependencies.                                                                                     #
# ########################################################################################################
find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 REQUIRED)
find_package(rclcpp REQUIRED)

# ########################################################################################################
# Add nodes and libraries.                                                                               #
# ########################################################################################################
set(target_name_cpp "${PROJECT_NAME}")
set(export_name_cpp "${target_name_cpp}_targets")
add_library("${target_name_cpp}" SHARED
  "src/extract_parameters.cpp"
  "src/get_agent_name.cpp"
  "src/sanitize_agent_name.cpp")
target_include_directories("${target_name_cpp}" PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features("${target_name_cpp}" PUBLIC c_std_99 cxx_std_17)
ament_target_dependencies("${target_name_cpp}" PUBLIC
  rclcpp
)

install(DIRECTORY
  "include/"
  DESTINATION "include"
)
ament_export_include_directories("include")
install(
  TARGETS "${target_name_cpp}"
  EXPORT "${export_name_cpp}"
  LIBRARY DESTINATION "lib"
  ARCHIVE DESTINATION "lib"
  RUNTIME DESTINATION "bin"
  INCLUDES DESTINATION "include"
)
ament_export_libraries("${target_name_cpp}")
ament_export_targets("${export_name_cpp}")

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

# ########################################################################################################
# Add other files/directories.                                                                           #
# ########################################################################################################
install(DIRECTORY
  "cmake"
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

ament_package(CONFIG_EXTRAS "cmake/common.cmake")