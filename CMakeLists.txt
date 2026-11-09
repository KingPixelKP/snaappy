cmake_minimum_required(VERSION 3.25)
project(snaappy VERSION 0.1.0 LANGUAGES CXX)

string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UPPER)
set(${PROJECT_NAME_UPPER}_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

option(${PROJECT_NAME_UPPER}_BUILD_TESTS "Build tests" ON)
option(${PROJECT_NAME_UPPER}_BUILD_EXAMPLES "Build example programs" ON)
option(${PROJECT_NAME_UPPER}_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(${PROJECT_NAME_UPPER}_ENABLE_CLANG_TIDY "Run clang-tidy during builds" OFF)
option(${PROJECT_NAME_UPPER}_ENABLE_CPPCHECK "Run cppcheck during builds" OFF)
option(${PROJECT_NAME_UPPER}_ENABLE_COVERAGE "Enable coverage instrumentation" OFF)
option(${PROJECT_NAME_UPPER}_BUILD_DOCS "Build documentation" ON)
option(${PROJECT_NAME_UPPER}_INSTALL "Enable install and package export rules" ON)

if(${PROJECT_NAME_UPPER}_INSTALL)
  include(GNUInstallDirs)
  include(CMakePackageConfigHelpers)
endif()

if(${PROJECT_NAME_UPPER}_BUILD_TESTS)
  enable_testing()
  include(GoogleTest)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Nix's compiler wrapper injects dependency include dirs via NIX_CFLAGS_COMPILE,
# so CMake sees them as implicit and omits them from compile_commands.json,
# leaving clangd unable to find fmt/gtest headers. Keep them explicit.
list(FILTER CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES EXCLUDE REGEX "^/nix/store/")

if(CMAKE_EXPORT_COMPILE_COMMANDS)
  file(CREATE_LINK
    "${CMAKE_BINARY_DIR}/compile_commands.json"
    "${CMAKE_SOURCE_DIR}/compile_commands.json"
    SYMBOLIC
  )
endif()

# includes
set(${PROJECT_NAME_UPPER}_COMPILE_DEFINITIONS -Wall -Wextra -Wpedantic -Werror)
set(${PROJECT_NAME_UPPER}_LINKER_DEFINITIONS "")
set(${PROJECT_NAME_UPPER}_SANITIZERS
  ""
  CACHE STRING
  "Comma-separated list of sanitizers (e.g. address,undefined,thread)"
)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_program(CCACHE_PROGRAM ccache)

if(CCACHE_PROGRAM)
  set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
else()
  message(STATUS "ccache not found; compiler launcher will not be configured")
endif()

# Cookie-Cmake helpers (package(), workspace(), tooling). The flake pins the
# source; `nix develop` exports COOKIE_CMAKE_DIR and `nix build` passes it.
set(COOKIE_CMAKE_DIR "$ENV{COOKIE_CMAKE_DIR}" CACHE PATH "Cookie-Cmake checkout (provided by nix develop / nix build)")

# The dev shell's value wins over the cache, so a configure run outside the
# shell (empty value) or before `nix flake update` (old store path) can't stick.
if(DEFINED ENV{COOKIE_CMAKE_DIR} AND NOT "$ENV{COOKIE_CMAKE_DIR}" STREQUAL "${COOKIE_CMAKE_DIR}")
  set(COOKIE_CMAKE_DIR "$ENV{COOKIE_CMAKE_DIR}" CACHE PATH "Cookie-Cmake checkout (provided by nix develop / nix build)" FORCE)
endif()

if(NOT EXISTS "${COOKIE_CMAKE_DIR}/cmake/cookie.cmake")
  message(FATAL_ERROR
    "Cookie-Cmake not found (COOKIE_CMAKE_DIR='${COOKIE_CMAKE_DIR}'). "
    "Configure from inside `nix develop`."
  )
endif()

list(APPEND CMAKE_MODULE_PATH "${COOKIE_CMAKE_DIR}/cmake")
include(cookie)

# Tools
configure_project_tooling()

# Dependencies: find_package() for every entry in deps.json (Nix provides them, see package.nix)
dependencies()

if(${PROJECT_NAME_UPPER}_BUILD_DOCS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/docs/CMakeLists.txt")
  add_subdirectory(docs)
endif()

# Find workspace members
set(EXCLUDED_WORKSPACE_MEMBERS
  src
  include
  cmake
  test
  docs
)

workspace(
  DISCOVER
  libs
  EXCLUDE
  ${EXCLUDED_WORKSPACE_MEMBERS}
)



# Main Executable
package(
  NAME ${PROJECT_NAME}
  BIN
  BIN_DEPS
  
  math
  
  fmt::fmt
)


if(${PROJECT_NAME_UPPER}_INSTALL)
  set(_package_config_install_dir "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
  set(_project_install_component "${PROJECT_NAME}")

  configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/project-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION "${_package_config_install_dir}"
  )

  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  install(
    EXPORT ${PROJECT_NAME}Targets
    FILE "${PROJECT_NAME}Targets.cmake"
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION "${_package_config_install_dir}"
    COMPONENT "${_project_install_component}"
  )

  install(
    FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION "${_package_config_install_dir}"
    COMPONENT "${_project_install_component}"
  )
endif()

add_coverage_report_target()
add_format_target()
