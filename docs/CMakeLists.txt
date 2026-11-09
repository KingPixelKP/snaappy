message(STATUS "Configuring \"Docs\"")

find_package(Doxygen REQUIRED)

set(PROJECT_ROOT_DIR "${${PROJECT_NAME_UPPER}_ROOT_DIR}")
set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/docs")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
    @ONLY
)

add_custom_target(${PROJECT_NAME}_docs
    COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
    WORKING_DIRECTORY "${PROJECT_ROOT_DIR}"
    COMMENT "Generating API documentation"
    VERBATIM
)
