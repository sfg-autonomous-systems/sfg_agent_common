macro(sfg_utils__add_executable EXECUTABLE_NAME)
    sfg_utils__add_internal("${EXECUTABLE_NAME}" EXECUTABLE ${ARGN})
endmacro()