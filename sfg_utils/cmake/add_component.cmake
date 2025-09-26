include("${CMAKE_CURRENT_LIST_DIR}/add_library_internal.cmake")

macro(sfg_utils__add_component EXECUTABLE_NAME PLUGIN_CLASS_NAME)
    cmake_parse_arguments(
        "ARG"
        ""
        "EXECUTOR"
        "SOURCES;AMENT_DEPENDENCIES;SYSTEM_DEPENDENCIES;EXPORT_DEPENDENCIES;ADDITIONAL_INSTALL_TARGETS"
        "${ARGN}"
    )

    set(target_name "${EXECUTABLE_NAME}_component_library")
    sfg_utils__add_library_internal("${target_name}"
        SOURCES "${ARG_SOURCES}"
        AMENT_DEPENDENCIES ${ARG_AMENT_DEPENDENCIES}
        SYSTEM_DEPENDENCIES ${ARG_SYSTEM_DEPENDENCIES}
        EXPORT_DEPENDENCIES ${ARG_EXPORT_DEPENDENCIES}
        ADDITIONAL_INSTALL_TARGETS ${ARG_ADDITIONAL_INSTALL_TARGETS}
    )
    rclcpp_components_register_node(
        "${target_name}"
        PLUGIN "${PLUGIN_CLASS_NAME}"
        EXECUTABLE "${EXECUTABLE_NAME}"
        EXECUTOR "${ARG_EXECUTOR}"
    )
endmacro()