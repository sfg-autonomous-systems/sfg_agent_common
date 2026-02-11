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

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/")
        install(
            DIRECTORY "include/"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        )
    endif()

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

        install(DIRECTORY "${include_dir}/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    endforeach()
endmacro()