# Adds a nixpkgs library to deps.json, like `cargo add`. Run through `make add`:
#
#   make add DEP=spdlog                  # library used by the build
#   make add DEP=catch2_3 KIND=test      # only when tests are built
#   make add DEP=qt6.qtbase PACKAGE=Qt6 COMPONENTS="Core Widgets"
#   make add DEP=fmt VERSION="11...<13"  # fail configuring outside this range
#
# The script builds the package with the flake's locked nixpkgs, finds its CMake
# config file to learn the find_package name, and prints the targets it exports.

cmake_minimum_required(VERSION 3.25)

if(NOT DEP)
  message(FATAL_ERROR "Usage: make add DEP=<nixpkgs attribute> [KIND=build|test|bench] [PACKAGE=<find_package name>] [VERSION=<min or range>] [COMPONENTS=\"A B\"]")
endif()

if(NOT KIND)
  set(KIND build)
endif()

if(NOT KIND MATCHES "^(build|test|bench)$")
  message(FATAL_ERROR "KIND must be build, test or bench (got \"${KIND}\")")
endif()

cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH _root)
set(_manifest "${_root}/deps.json")
file(READ "${_manifest}" _json)

string(JSON _existing ERROR_VARIABLE _missing GET "${_json}" "${DEP}")
if(NOT _missing)
  message(FATAL_ERROR "\"${DEP}\" is already in deps.json")
endif()

message(STATUS "Fetching nixpkgs#${DEP} (locked nixpkgs)...")
execute_process(
  COMMAND nix build --no-link --print-out-paths --inputs-from "${_root}" "nixpkgs#${DEP}^*"
  OUTPUT_VARIABLE _outputs
  ERROR_VARIABLE _nix_error
  RESULT_VARIABLE _nix_result
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT _nix_result EQUAL 0)
  message(FATAL_ERROR "nix could not build nixpkgs#${DEP}:\n${_nix_error}\nSearch for the attribute at https://search.nixos.org/packages")
endif()

string(REPLACE "\n" ";" _outputs "${_outputs}")

# CMake config files the package installs, and the targets they export.
set(_packages)
set(_targets)
foreach(_output IN LISTS _outputs)
  file(GLOB_RECURSE _configs
    "${_output}/lib/cmake/*Config.cmake"
    "${_output}/lib/cmake/*-config.cmake"
    "${_output}/lib64/cmake/*Config.cmake"
    "${_output}/lib64/cmake/*-config.cmake"
    "${_output}/share/*Config.cmake"
    "${_output}/share/*-config.cmake"
  )

  foreach(_config IN LISTS _configs)
    cmake_path(GET _config FILENAME _file)
    string(REGEX REPLACE "(Config|-config)\\.cmake$" "" _package "${_file}")
    list(APPEND _packages "${_package}")
  endforeach()

  file(GLOB_RECURSE _target_files "${_output}/lib/cmake/*.cmake" "${_output}/lib64/cmake/*.cmake" "${_output}/share/*.cmake")
  foreach(_target_file IN LISTS _target_files)
    file(STRINGS "${_target_file}" _lines REGEX "add_library\\([A-Za-z0-9_.+-]+::[A-Za-z0-9_.+-]+ ")
    foreach(_line IN LISTS _lines)
      string(REGEX MATCH "add_library\\(([A-Za-z0-9_.+-]+::[A-Za-z0-9_.+-]+) " _ "${_line}")
      list(APPEND _targets "${CMAKE_MATCH_1}")
    endforeach()
  endforeach()
endforeach()

list(REMOVE_DUPLICATES _packages)
list(REMOVE_DUPLICATES _targets)

if(NOT PACKAGE)
  list(LENGTH _packages _count)

  if(_count EQUAL 0)
    message(FATAL_ERROR
      "nixpkgs#${DEP} installs no CMake config file, so find_package(... CONFIG) cannot find it. "
      "It may only ship pkg-config files."
    )
  elseif(_count EQUAL 1)
    set(PACKAGE "${_packages}")
  else()
    # Several configs (e.g. Qt modules): take the one named like the attribute.
    string(REGEX REPLACE "[^a-z0-9]" "" _wanted "${DEP}")
    string(TOLOWER "${_wanted}" _wanted)
    foreach(_package IN LISTS _packages)
      string(TOLOWER "${_package}" _candidate)
      string(REGEX REPLACE "[^a-z0-9]" "" _candidate "${_candidate}")
      if(_candidate STREQUAL _wanted)
        set(PACKAGE "${_package}")
      endif()
    endforeach()

    if(NOT PACKAGE)
      list(JOIN _packages ", " _choices)
      message(FATAL_ERROR "nixpkgs#${DEP} provides several CMake packages (${_choices}). Pick one with PACKAGE=<name>.")
    endif()
  endif()
endif()

# Rewrite deps.json with one entry per line.
set(_entry "\"package\": \"${PACKAGE}\"")
if(NOT KIND STREQUAL "build")
  string(APPEND _entry ", \"kind\": \"${KIND}\"")
endif()
if(VERSION)
  string(APPEND _entry ", \"version\": \"${VERSION}\"")
endif()
if(COMPONENTS)
  separate_arguments(_components UNIX_COMMAND "${COMPONENTS}")
  list(JOIN _components "\", \"" _components)
  string(APPEND _entry ", \"components\": [\"${_components}\"]")
endif()
set(_entry "{ ${_entry} }")

string(JSON _json SET "${_json}" "${DEP}" "${_entry}")
string(JSON _count LENGTH "${_json}")
math(EXPR _last "${_count} - 1")

set(_lines)
foreach(_index RANGE ${_last})
  string(JSON _name MEMBER "${_json}" ${_index})
  string(JSON _value GET "${_json}" "${_name}")
  string(REGEX REPLACE "\n[ ]*" " " _value "${_value}")
  string(REPLACE " : " ": " _value "${_value}")
  list(APPEND _lines "  \"${_name}\": ${_value}")
endforeach()

list(JOIN _lines ",\n" _body)
file(WRITE "${_manifest}" "{\n${_body}\n}\n")

message(STATUS "Added ${DEP} to deps.json (find_package(${PACKAGE}), kind ${KIND})")
if(_targets)
  list(JOIN _targets " " _targets)
  message(STATUS "Link it from package(), e.g. DEPS <target>. Exported targets: ${_targets}")
endif()
