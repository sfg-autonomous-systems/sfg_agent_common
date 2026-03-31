macro(sfg_utils__add_internal TARGET_NAME)
    cmake_parse_arguments(
        "ARG"
        "EXECUTABLE"
        ""
        "SOURCES;ADDITIONAL_INCLUDE_DIRS;AMENT_DEPENDENCIES;SYSTEM_DEPENDENCIES;EXPORT_DEPENDENCIES;ADDITIONAL_INSTALL_TARGETS"
        "${ARGN}"
    )

    include(GNUInstallDirs)

    set(target_name "${TARGET_NAME}")
    set(export_name "${target_name}_targets")

    if(ARG_EXECUTABLE)
        add_executable("${target_name}" "${ARG_SOURCES}")
    else()
        add_library("${target_name}" SHARED "${ARG_SOURCES}")
    endif()

    target_compile_features("${target_name}" PUBLIC c_std_99 cxx_std_17)

    if(ARG_EXECUTABLE)
        target_include_directories("${target_name}" PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        )
    else()
        target_include_directories("${target_name}" PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        )
    endif()

    set(SCOPE "PUBLIC")
    set(IS_SYSTEM "")

    foreach(include_dir IN LISTS ARG_ADDITIONAL_INCLUDE_DIRS)
        if("${include_dir}" STREQUAL "PUBLIC" OR "${include_dir}" STREQUAL "INTERFACE" OR "${include_dir}" STREQUAL "PRIVATE")
            set(SCOPE "${include_dir}")
            continue()
        endif()

        if("${include_dir}" STREQUAL "SYSTEM")
            set(IS_SYSTEM "SYSTEM")
            continue()
        endif()

        if("${include_dir}" STREQUAL "NO_SYSTEM")
            set(IS_SYSTEM "")
            continue()
        endif()

        if(ARG_EXECUTABLE)
            target_include_directories("${target_name}" ${IS_SYSTEM} "${SCOPE}"
                $<BUILD_INTERFACE:${include_dir}>
            )
        else()
            target_include_directories("${target_name}" ${IS_SYSTEM} "${SCOPE}"
                $<BUILD_INTERFACE:${include_dir}>
                $<INSTALL_INTERFACE:include>
            )
        endif()
    endforeach()

    if(ARG_AMENT_DEPENDENCIES)
        ament_target_dependencies("${target_name}" PUBLIC ${ARG_AMENT_DEPENDENCIES})
    endif()

    set(SCOPE "PUBLIC")

    foreach(dependency IN LISTS ARG_SYSTEM_DEPENDENCIES)
        if("${dependency}" STREQUAL "PUBLIC" OR "${dependency}" STREQUAL "INTERFACE" OR "${dependency}" STREQUAL "PRIVATE")
            set(SCOPE "${dependency}")
            continue()
        endif()

        target_link_libraries("${target_name}" "${SCOPE}" "${dependency}")
    endforeach()

    if(ARG_EXECUTABLE)
        install(
            TARGETS "${target_name}" ${ARG_ADDITIONAL_INSTALL_TARGETS}
            DESTINATION "lib/${PROJECT_NAME}"
        )
    else()
        ament_export_targets("${export_name}" HAS_LIBRARY_TARGET)

        if(ARG_EXPORT_DEPENDENCIES)
            ament_export_dependencies(${ARG_EXPORT_DEPENDENCIES})
        endif()

        install(
            TARGETS "${target_name}" ${ARG_ADDITIONAL_INSTALL_TARGETS}
            EXPORT "${export_name}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        )

        install(
            DIRECTORY "include/"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
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

            install(DIRECTORY "${include_dir}/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
        endforeach()
    endif()
endmacro()