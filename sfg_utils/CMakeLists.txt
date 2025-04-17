cmake_minimum_required(VERSION 3.8)
project(sfg_utils)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ########################################################################################################
# Find dependencies.                                                                                     #
# ########################################################################################################
find_package(ament_cmake_auto REQUIRED)

# ########################################################################################################
# Add nodes and libraries.                                                                               #
# ########################################################################################################
set(target "${PROJECT_NAME}")
add_library("${target}" SHARED "src/sanitize_hostname.cpp")
target_include_directories("${target}" PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features("${target}" PUBLIC c_std_99 cxx_std_17)
ament_target_dependencies(${target})

install(DIRECTORY
  "include/"
  DESTINATION "include"
)

install(
  TARGETS ${target}
  EXPORT ${target}
  LIBRARY DESTINATION "lib"
  ARCHIVE DESTINATION "lib"
  RUNTIME DESTINATION "bin"
  INCLUDES DESTINATION "include"
)

ament_export_include_directories("include")
ament_export_libraries(${target})

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