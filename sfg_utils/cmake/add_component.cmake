macro(sfg_utils__add_component EXECUTABLE_NAME PLUGIN_CLASS_NAME)
    cmake_parse_arguments(
        "ARG"
        ""
        ""
        "SOURCES;AMENT_DEPENDENCIES;SYSTEM_DEPENDENCIES"
        "${ARGN}"
    )

    set(target_name "${EXECUTABLE_NAME}_component")
    set(export_name "${target_name}_targets")

    add_library("${target_name}" SHARED "${ARG_SOURCES}")

    rclcpp_components_register_node(
        "${target_name}"
        PLUGIN "${PLUGIN_CLASS_NAME}"
        EXECUTABLE "${EXECUTABLE_NAME}"
    )

    target_include_directories("${target_name}" PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    target_compile_features("${target_name}" PUBLIC c_std_99 cxx_std_17)

    set(CURRENT_SCOPE "PUBLIC")

    foreach(dependency IN LISTS ARG_AMENT_DEPENDENCIES)
        if("${dependency}" STREQUAL "PUBLIC" OR "${dependency}" STREQUAL "INTERFACE" OR "${dependency}" STREQUAL "PRIVATE")
            set(CURRENT_SCOPE "${dependency}")
            continue()
        endif()

        ament_target_dependencies("${target_name}" "${CURRENT_SCOPE}" "${dependency}")
    endforeach()

    set(CURRENT_SCOPE "PUBLIC")

    foreach(dependency IN LISTS ARG_SYSTEM_DEPENDENCIES)
        if("${dependency}" STREQUAL "PUBLIC" OR "${dependency}" STREQUAL "INTERFACE" OR "${dependency}" STREQUAL "PRIVATE")
            set(CURRENT_SCOPE "${dependency}")
            continue()
        endif()

        target_link_libraries("${target_name}" "${CURRENT_SCOPE}" "${dependency}")

        if("${CURRENT_SCOPE}" STREQUAL "PUBLIC" OR "${CURRENT_SCOPE}" STREQUAL "INTERFACE")
            ament_export_dependencies("${dependency}")
        endif()
    endforeach()

    ament_export_targets("${export_name}" HAS_LIBRARY_TARGET)

    install(
        TARGETS "${target_name}"
        EXPORT "${export_name}"
        LIBRARY DESTINATION "lib"
        ARCHIVE DESTINATION "lib"
        RUNTIME DESTINATION "bin"
        INCLUDES DESTINATION "include"
    )
endmacro()