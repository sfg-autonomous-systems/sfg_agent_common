include("${CMAKE_CURRENT_LIST_DIR}/add_library_internal.cmake")

macro(sfg_utils__add_library LIBRARY_NAME)
    cmake_parse_arguments(
        "ARG"
        ""
        ""
        "SOURCES;AMENT_DEPENDENCIES;SYSTEM_DEPENDENCIES;EXPORT_DEPENDENCIES;ADDITIONAL_INSTALL_TARGETS"
        "${ARGN}"
    )

    set(target_name "${LIBRARY_NAME}")
    sfg_utils__add_library_internal("${target_name}"
        SOURCES "${ARG_SOURCES}"
        AMENT_DEPENDENCIES ${ARG_AMENT_DEPENDENCIES}
        SYSTEM_DEPENDENCIES ${ARG_SYSTEM_DEPENDENCIES}
        EXPORT_DEPENDENCIES ${ARG_EXPORT_DEPENDENCIES}
        ADDITIONAL_INSTALL_TARGETS ${ARG_ADDITIONAL_INSTALL_TARGETS}
    )
    install(DIRECTORY
        "include/"
        DESTINATION "include"
    )
    ament_export_include_directories("include")
    ament_export_libraries("${target_name}")
endmacro()