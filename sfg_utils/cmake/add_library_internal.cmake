macro(sfg_utils__add_library_internal TARGET_NAME)
    cmake_parse_arguments(
        "ARG"
        ""
        ""
        "SOURCES;AMENT_DEPENDENCIES;SYSTEM_DEPENDENCIES;EXPORT_DEPENDENCIES;ADDITIONAL_INSTALL_TARGETS"
        "${ARGN}"
    )

    set(target_name "${TARGET_NAME}")
    set(export_name "${target_name}_targets")

    add_library("${target_name}" SHARED "${ARG_SOURCES}")
    target_compile_features("${target_name}" PUBLIC c_std_99 cxx_std_17)
    target_include_directories("${target_name}" PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    ament_export_targets("${export_name}" HAS_LIBRARY_TARGET)

    foreach(dependency IN LISTS ARG_AMENT_DEPENDENCIES)
        ament_target_dependencies("${target_name}" PUBLIC "${dependency}")
    endforeach()

    set(CURRENT_SCOPE "PUBLIC")

    foreach(dependency IN LISTS ARG_SYSTEM_DEPENDENCIES)
        if("${dependency}" STREQUAL "PUBLIC" OR "${dependency}" STREQUAL "INTERFACE" OR "${dependency}" STREQUAL "PRIVATE")
            set(CURRENT_SCOPE "${dependency}")
            continue()
        endif()

        target_link_libraries("${target_name}" "${CURRENT_SCOPE}" "${dependency}")
    endforeach()

    ament_export_dependencies("${package_name}")

    if(ARG_EXPORT_DEPENDENCIES)
        ament_export_dependencies(${ARG_EXPORT_DEPENDENCIES})
    endif()

    install(
        TARGETS "${target_name}" ${ARG_ADDITIONAL_INSTALL_TARGETS}
        EXPORT "${export_name}"
        LIBRARY DESTINATION "lib"
        ARCHIVE DESTINATION "lib"
        RUNTIME DESTINATION "bin"
        INCLUDES DESTINATION "include"
    )
endmacro()