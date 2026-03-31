include("${CMAKE_CURRENT_LIST_DIR}/add_internal.cmake")

macro(sfg_utils__add_library LIBRARY_NAME)
    sfg_utils__add_internal("${LIBRARY_NAME}" ${ARGN})
endmacro()