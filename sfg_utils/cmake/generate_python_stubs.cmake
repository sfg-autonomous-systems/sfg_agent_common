function(generate_python_stubs TARGET_NAME)
    cmake_parse_arguments(
        "ARG"
        ""
        "OUTPUT_DIRECTORY"
        "STUBGEN_ARGS"
        "${ARGN}"
    )

    find_program(PYBIND11_STUBGEN_EXECUTABLE pybind11-stubgen)

    if(NOT PYBIND11_STUBGEN_EXECUTABLE)
        message(WARNING "Cannot create python stub file because pybind11-stubgen was not found. Please run 'pip install pybind11-stubgen'.")
        return()
    endif()

    set(target_name "generate_python_stubs_for_${TARGET_NAME}")
    set(stub_directory "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}")

    add_custom_target("${target_name}" ALL
        COMMAND "${Python3_EXECUTABLE}" -m pybind11_stubgen -o "${CMAKE_CURRENT_BINARY_DIR}" ${ARG_STUBGEN_ARGS} "${TARGET_NAME}"
        DEPENDS "${TARGET_NAME}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Generating python stubs for ${TARGET_NAME}."
    )

    if(ARG_OUTPUT_DIRECTORY)
        set(output_directory "${PYTHON_INSTALL_DIR}/${ARG_OUTPUT_DIRECTORY}")
    else()
        set(output_directory "${PYTHON_INSTALL_DIR}/${TARGET_NAME}")
    endif()

    INSTALL(
        DIRECTORY "${stub_directory}/"
        DESTINATION "${output_directory}"
        FILES_MATCHING PATTERN "*.pyi"
    )

    set(RESTRUCTURE_PYTHON_STUBS_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/restructure_python_stubs_for_${TARGET_NAME}.cmake")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/restructure_python_stubs.cmake.in" "${RESTRUCTURE_PYTHON_STUBS_SCRIPT}" @ONLY)
    install(SCRIPT "${RESTRUCTURE_PYTHON_STUBS_SCRIPT}")
endfunction()