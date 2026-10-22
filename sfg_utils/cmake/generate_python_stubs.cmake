# ########################################################################################################
# pybind11-stubgen.                                                                                      #
# ########################################################################################################
# pybind11-stubgen is a pure python package, so it does not need to be installed and can be run straight
# from a source checkout. It is not available via rosdep or apt. An already importable copy (e.g. from
# `pip install pybind11-stubgen`) is preferred; otherwise a pinned checkout is fetched into the build
# directory. Set PYBIND11_STUBGEN_AUTO_FETCH to OFF to never download it.
set(PYBIND11_STUBGEN_VERSION "v3.0.0" CACHE STRING
    "pybind11-stubgen git tag to fetch when it is not already importable."
)
option(PYBIND11_STUBGEN_AUTO_FETCH
    "Download pybind11-stubgen when it is not already importable." ON
)

function(generate_python_stubs TARGET_NAME)
    cmake_parse_arguments(
        "ARG"
        ""
        "OUTPUT_DIRECTORY"
        "STUBGEN_ARGS"
        "${ARGN}"
    )

    if(NOT Python3_EXECUTABLE)
        find_package(Python3 COMPONENTS Interpreter QUIET)
    endif()

    if(NOT Python3_EXECUTABLE)
        message(WARNING "Cannot create python stub file because no python interpreter was found.")
        return()
    endif()

    # Prefer an already importable pybind11_stubgen, e.g. from pip.
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -c "import pybind11_stubgen"
        RESULT_VARIABLE stubgen_import_result
        OUTPUT_QUIET
        ERROR_QUIET
    )

    if(NOT stubgen_import_result EQUAL 0)
        if(NOT PYBIND11_STUBGEN_AUTO_FETCH)
            message(WARNING
                "Cannot create python stub file because pybind11-stubgen is not available. "
                "Install it with 'pip install pybind11-stubgen', or enable "
                "-DPYBIND11_STUBGEN_AUTO_FETCH=ON to download it."
            )
            return()
        endif()

        # FetchContent is a no-op when the source directory already exists, so
        # this only downloads once per build directory.
        include(FetchContent)
        FetchContent_Declare(pybind11_stubgen
            GIT_REPOSITORY "https://github.com/pybind/pybind11-stubgen.git"
            GIT_TAG "${PYBIND11_STUBGEN_VERSION}"
            GIT_SHALLOW TRUE
            # The project is only used as a python module. Pointing SOURCE_SUBDIR
            # at a non-existent directory makes FetchContent_MakeAvailable skip
            # add_subdirectory() instead of trying to build the project.
            SOURCE_SUBDIR "cmake/do-not-add-subdirectory"
        )
        FetchContent_MakeAvailable(pybind11_stubgen)
        list(APPEND stubgen_pythonpath "${pybind11_stubgen_SOURCE_DIR}")
    endif()

    # Keep the PYTHONPATH of the current environment.
    if(DEFINED ENV{PYTHONPATH} AND NOT "$ENV{PYTHONPATH}" STREQUAL "")
        list(APPEND stubgen_pythonpath "$ENV{PYTHONPATH}")
    endif()
    list(JOIN stubgen_pythonpath ":" stubgen_pythonpath)

    set(target_name "generate_python_stubs_for_${TARGET_NAME}")
    set(stub_directory "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}")

    add_custom_target("${target_name}" ALL
        COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=${stubgen_pythonpath}"
                "${Python3_EXECUTABLE}" -m pybind11_stubgen
                -o "${CMAKE_CURRENT_BINARY_DIR}" ${ARG_STUBGEN_ARGS} "${TARGET_NAME}"
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