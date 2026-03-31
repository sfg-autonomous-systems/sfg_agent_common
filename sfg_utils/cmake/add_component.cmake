include("${CMAKE_CURRENT_LIST_DIR}/add_internal.cmake")

macro(sfg_utils__add_component EXECUTABLE_NAME PLUGIN_CLASS_NAME)
    cmake_parse_arguments(
        "ARG"
        ""
        "EXECUTOR"
        ""
        "${ARGN}"
    )

    set(target_name "${EXECUTABLE_NAME}_component_library")
    sfg_utils__add_internal("${target_name}" ${ARGN})
    rclcpp_components_register_node(
        "${target_name}"
        PLUGIN "${PLUGIN_CLASS_NAME}"
        EXECUTABLE "${EXECUTABLE_NAME}"
        EXECUTOR "${ARG_EXECUTOR}"
    )
endmacro()