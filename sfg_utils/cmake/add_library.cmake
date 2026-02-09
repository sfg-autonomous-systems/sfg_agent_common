include("${CMAKE_CURRENT_LIST_DIR}/add_library_internal.cmake")

macro(sfg_utils__add_library LIBRARY_NAME)
    cmake_parse_arguments(
        "ARG"
        ""
        ""
        "SOURCES;ADDITIONAL_INCLUDE_DIRS;AMENT_DEPENDENCIES;SYSTEM_DEPENDENCIES;EXPORT_DEPENDENCIES;ADDITIONAL_INSTALL_TARGETS"
        "${ARGN}"
    )

    set(target_name "${LIBRARY_NAME}")
    sfg_utils__add_library_internal("${target_name}"
        SOURCES "${ARG_SOURCES}"
        ADDITIONAL_INCLUDE_DIRS ${ARG_ADDITIONAL_INCLUDE_DIRS}
        AMENT_DEPENDENCIES ${ARG_AMENT_DEPENDENCIES}
        SYSTEM_DEPENDENCIES ${ARG_SYSTEM_DEPENDENCIES}
        EXPORT_DEPENDENCIES ${ARG_EXPORT_DEPENDENCIES}
        ADDITIONAL_INSTALL_TARGETS ${ARG_ADDITIONAL_INSTALL_TARGETS}
    )
    install(
        DIRECTORY "include/"
        DESTINATION "include"
    )

    # Install the additional public include directories, too.
    set(SCOPE "PUBLIC")

    foreach(include_dir IN LISTS ARG_ADDITIONAL_INCLUDE_DIRS)
        if("${include_dir}" STREQUAL "PUBLIC" OR "${include_dir}" STREQUAL "INTERFACE" OR "${include_dir}" STREQUAL "PRIVATE")
            set(SCOPE "${include_dir}")
            continue()
        endif()

        if("${SCOPE}" STREQUAL "PRIVATE")
            continue()
        endif()

        install(
            DIRECTORY "${include_dir}/"
            DESTINATION "include"
        )
    endforeach()

    ament_export_include_directories("include")
    ament_export_libraries("${target_name}")
endmacro()