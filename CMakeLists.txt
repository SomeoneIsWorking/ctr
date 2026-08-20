cmake_minimum_required(VERSION 3.24)
project(ctr_port LANGUAGES C CXX)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# PSXPORT_DIR — the framework. external/psxport is NOT a git submodule (2026-08-16): it is a symlink
# to the workspace's shared framework clone, or a private clone at this repo's psxport.pin on a fresh
# machine; tools/psxport_sync.py --auto establishes whichever applies. Defaults to external/psxport so
# a bare clone of this repo builds standalone.
set(PSXPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/psxport" CACHE PATH
    "psxport framework source directory (default: external/psxport)")
if(NOT EXISTS "${PSXPORT_DIR}/cmake/psxport.cmake")
  message(FATAL_ERROR "PSXPORT_DIR=${PSXPORT_DIR} has no cmake/psxport.cmake — run "
                      "`python3 tools/psxport_sync.py --auto`, or point PSXPORT_DIR at a psxport checkout.")
endif()
# RECORD WHAT THIS CONFIGURE RESOLVED, so `tools/psxport_sync.py --check` can compare the built
# framework against the one psxport.pin RECORDS.
execute_process(COMMAND git -C "${PSXPORT_DIR}" rev-parse HEAD
                OUTPUT_VARIABLE PSXPORT_RESOLVED_SHA OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET RESULT_VARIABLE _psxport_rev_rc)
if(NOT _psxport_rev_rc EQUAL 0)
  set(PSXPORT_RESOLVED_SHA "unknown")
endif()
file(WRITE "${CMAKE_BINARY_DIR}/psxport_resolved.txt"
     "dir = ${PSXPORT_DIR}\ncommit = ${PSXPORT_RESOLVED_SHA}\n")
set(PSXPORT_BUILD_SMOKE ON CACHE BOOL "Build the framework agnosticism smoke" FORCE)
add_subdirectory("${PSXPORT_DIR}" psxport_build)
add_custom_target(ctr_scaffold DEPENDS psxport_smoke)
add_custom_target(
  cpp_policy
  COMMAND
    "${Python3_EXECUTABLE}" "${PSXPORT_DIR}/tools/check_cpp_style.py"
    --root "${CMAKE_CURRENT_SOURCE_DIR}"
    --compile-commands "${CMAKE_BINARY_DIR}"
    --allow-empty-scaffold
  USES_TERMINAL
  VERBATIM)
add_custom_target(
  provision_selftest
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/provision.py" --selftest
  DEPENDS discdump
  USES_TERMINAL
  VERBATIM)
add_custom_target(
  oracle_boot_check
  COMMAND
    "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/provision.py"
    --discdump "$<TARGET_FILE:discdump>"
  COMMAND "$<TARGET_FILE:oracle_spike>"
  COMMAND
    "${Python3_EXECUTABLE}" "${PSXPORT_DIR}/tools/oracle/crossvalidate_crt0.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/scratch/raw/ctr/SCUS_944.26"
    --build "${CMAKE_BINARY_DIR}/psxport_build"
    --steps 400000
  DEPENDS crt0_extract discdump oracle_spike oracle_trace
  USES_TERMINAL
  VERBATIM)
add_custom_target(
  verify
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/psxport_sync.py" --check
  COMMAND "$<TARGET_FILE:psxport_smoke>"
  DEPENDS cpp_policy provision_selftest psxport_smoke
  USES_TERMINAL
  VERBATIM)
