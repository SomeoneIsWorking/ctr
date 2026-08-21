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
include(CTest)

add_test(
  NAME ctr_provision_selftest
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/provision.py" --selftest)
add_test(
  NAME ctr_crt0_compare_selftest
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/compare_crt0_trace.py" --selftest)

add_custom_target(
  ctr04_compare_selftest
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/compare_crt0_trace.py" --selftest
  USES_TERMINAL
  VERBATIM)

# Compile the harness in an asset-free clone as well. Linking it requires the gitignored generated
# registry, but format/tidy must retain a real compile command before that substrate exists.
add_library(ctr_crt0_trace_compile OBJECT game/core/crt0_port_trace.cpp)
set_target_properties(
  ctr_crt0_trace_compile
  PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(ctr_crt0_trace_compile PRIVATE game/core)
target_link_libraries(ctr_crt0_trace_compile PRIVATE psxport)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/rec_sources.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/generated/rec_sources.cmake")
  list(TRANSFORM GEN_REC_SRCS PREPEND "generated/")
  set_source_files_properties(
    ${GEN_REC_SRCS}
    PROPERTIES
      LANGUAGE CXX
      COMPILE_OPTIONS "-O1;-foptimize-sibling-calls;-fno-strict-aliasing;-fwrapv")

  add_executable(ctr_crt0_port_trace $<TARGET_OBJECTS:ctr_crt0_trace_compile> ${GEN_REC_SRCS})
  set_target_properties(
    ctr_crt0_port_trace
    PROPERTIES
      CXX_STANDARD 20
      CXX_STANDARD_REQUIRED ON
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scratch/bin")
  target_include_directories(ctr_crt0_port_trace PRIVATE game/core)
  target_link_libraries(ctr_crt0_port_trace PRIVATE psxport)

  add_custom_target(
    ctr04_check
    COMMAND
      "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/provision.py"
      --discdump "$<TARGET_FILE:discdump>"
    COMMAND
      "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/compare_crt0_trace.py"
      "${CMAKE_CURRENT_SOURCE_DIR}/scratch/raw/ctr/SCUS_944.26"
      --oracle-trace "$<TARGET_FILE:oracle_trace>"
      --port-trace "$<TARGET_FILE:ctr_crt0_port_trace>"
      --steps 400000
    COMMAND
      "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/compare_crt0_trace.py"
      "${CMAKE_CURRENT_SOURCE_DIR}/scratch/raw/ctr/SCUS_944.26"
      --oracle-trace "$<TARGET_FILE:oracle_trace>"
      --port-trace "$<TARGET_FILE:ctr_crt0_port_trace>"
      --steps 400000
      --force-port-field gp=0
      --expect-difference
    DEPENDS ctr_crt0_port_trace discdump oracle_trace
    USES_TERMINAL
    VERBATIM)
else()
  message(STATUS
    "CTR-04 generated trace target is unavailable: emit generated/ from the verified USA executable "
    "with the documented shipping-emitter command, then reconfigure.")
endif()
add_custom_target(
  cpp_policy
  COMMAND
    "${Python3_EXECUTABLE}" "${PSXPORT_DIR}/tools/check_cpp_style.py"
    --root "${CMAKE_CURRENT_SOURCE_DIR}"
    --compile-commands "${CMAKE_BINARY_DIR}"
  DEPENDS ctr_crt0_trace_compile
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
  DEPENDS cpp_policy provision_selftest ctr04_compare_selftest psxport_smoke
  USES_TERMINAL
  VERBATIM)
