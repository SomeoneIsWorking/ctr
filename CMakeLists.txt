cmake_minimum_required(VERSION 3.24)
project(ctr_port LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
include(CTest)

set(PSXPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/psxport" CACHE PATH
    "psxport framework source directory")
if(NOT EXISTS "${PSXPORT_DIR}/cmake/psxport.cmake")
  message(FATAL_ERROR
          "PSXPORT_DIR=${PSXPORT_DIR} has no cmake/psxport.cmake; run the Python framework resolver")
endif()
execute_process(
  COMMAND git -C "${PSXPORT_DIR}" rev-parse HEAD
  OUTPUT_VARIABLE PSXPORT_RESOLVED_SHA
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
  RESULT_VARIABLE _psxport_rev_rc
)
if(NOT _psxport_rev_rc EQUAL 0)
  set(PSXPORT_RESOLVED_SHA "unknown")
endif()
file(WRITE "${CMAKE_BINARY_DIR}/psxport_resolved.txt"
     "dir = ${PSXPORT_DIR}\ncommit = ${PSXPORT_RESOLVED_SHA}\n")

set(PSXPORT_BUILD_SMOKE ON CACHE BOOL "Build the framework agnosticism smoke" FORCE)
add_subdirectory("${PSXPORT_DIR}" psxport_build)

set(CTR_OVERLAY_CATALOG "${CMAKE_CURRENT_BINARY_DIR}/identity/ctr_overlay_catalog.h")
add_custom_command(
  OUTPUT "${CTR_OVERLAY_CATALOG}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/extract_overlays.py"
          --emit-header "${CTR_OVERLAY_CATALOG}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/titles/ctr/overlays.json"
          "${CMAKE_CURRENT_SOURCE_DIR}/tools/extract_overlays.py"
  VERBATIM
)

add_library(
  ctr_runtime STATIC
  "${CTR_OVERLAY_CATALOG}"
  game/core/async_disc_owner.cpp
  game/core/ctr_runtime.cpp
  game/core/dma_callback_owner.cpp
  game/core/frame_callback_owner.cpp
  game/core/frame_driver.cpp
  game/core/overlay_image_owner.cpp
  game/core/platform_hle_plan.cpp
  game/core/render_list_boundary_diagnostic.cpp
  game/core/runtime_composition.cpp
  game/video/presentation_owner.cpp
  game/video/projection_owner.cpp
)
target_include_directories(
  ctr_runtime
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/game/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/game/video"
    "${CMAKE_CURRENT_BINARY_DIR}/identity"
)
target_compile_features(ctr_runtime PUBLIC cxx_std_20)
target_link_libraries(ctr_runtime PUBLIC psxport)

add_library(ctr_command_line STATIC game/app/command_line.cpp)
target_include_directories(ctr_command_line PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/game/app")
target_compile_features(ctr_command_line PUBLIC cxx_std_20)

add_executable(ctr_port game/app/main.cpp)
target_compile_features(ctr_port PRIVATE cxx_std_20)
target_link_libraries(ctr_port PRIVATE ctr_command_line ctr_runtime)
set_target_properties(ctr_port PROPERTIES ENABLE_EXPORTS ON)
add_dependencies(ctr_port gen_gpu_shaders)

if(BUILD_TESTING)
  add_executable(ctr_command_line_test tests/test_command_line.cpp)
  target_compile_features(ctr_command_line_test PRIVATE cxx_std_20)
  target_link_libraries(ctr_command_line_test PRIVATE ctr_command_line)
  add_test(NAME ctr_command_line COMMAND ctr_command_line_test)

  add_executable(ctr_execution_exit_test tests/ctr_execution_exit.cpp)
  target_compile_features(ctr_execution_exit_test PRIVATE cxx_std_20)
  target_link_libraries(ctr_execution_exit_test PRIVATE ctr_runtime)
  add_test(NAME ctr_execution_exit COMMAND ctr_execution_exit_test)

  add_executable(ctr_disc_ownership_test tests/ctr_disc_ownership.cpp)
  target_compile_features(ctr_disc_ownership_test PRIVATE cxx_std_20)
  target_link_libraries(ctr_disc_ownership_test PRIVATE ctr_runtime)
  add_test(NAME ctr_disc_ownership COMMAND ctr_disc_ownership_test)

  add_executable(ctr_overlay_image_owner_test tests/ctr_overlay_image_owner.cpp)
  target_compile_features(ctr_overlay_image_owner_test PRIVATE cxx_std_20)
  target_link_libraries(ctr_overlay_image_owner_test PRIVATE ctr_runtime)
  add_test(NAME ctr_overlay_image_owner COMMAND ctr_overlay_image_owner_test)

  add_executable(ctr_frame_execution_test tests/ctr_frame_execution.cpp)
  target_compile_features(ctr_frame_execution_test PRIVATE cxx_std_20)
  target_link_libraries(ctr_frame_execution_test PRIVATE ctr_runtime)
  add_test(NAME ctr_frame_execution COMMAND ctr_frame_execution_test)
  set_tests_properties(ctr_frame_execution PROPERTIES TIMEOUT 20)

  add_executable(ctr_render_list_diagnostic_test tests/ctr_render_list_diagnostic.cpp)
  target_compile_features(ctr_render_list_diagnostic_test PRIVATE cxx_std_20)
  target_link_libraries(ctr_render_list_diagnostic_test PRIVATE ctr_runtime)
  add_test(NAME ctr_render_list_diagnostic COMMAND ctr_render_list_diagnostic_test)

  add_test(NAME ctr_launcher COMMAND "${Python3_EXECUTABLE}" -B
           "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_run.py")
  set_tests_properties(ctr_launcher PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
  add_test(NAME ctr_framework_resolver COMMAND "${Python3_EXECUTABLE}" -B
           "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_psxport_sync.py")
  set_tests_properties(ctr_framework_resolver PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
  add_test(NAME ctr_framework_pin COMMAND "${Python3_EXECUTABLE}"
           "${CMAKE_CURRENT_SOURCE_DIR}/tools/psxport_sync.py" --check
           --resolved "${CMAKE_BINARY_DIR}/psxport_resolved.txt")

  add_test(NAME ctr_help_without_assets COMMAND ctr_port --help)
  add_test(NAME ctr_short_help_without_assets COMMAND ctr_port -h)
  set_tests_properties(
    ctr_help_without_assets ctr_short_help_without_assets
    PROPERTIES PASS_REGULAR_EXPRESSION "Usage: .*ctr_port \\[--help\\]"
  )
  add_test(
    NAME ctr_execution_boundary
    COMMAND
      "${Python3_EXECUTABLE}" "${PSXPORT_DIR}/tools/check_execution_boundary.py"
      --root "${CMAKE_CURRENT_SOURCE_DIR}"
      --cmake "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
      --binary "$<TARGET_FILE:ctr_port>"
  )
endif()

add_custom_target(
  cpp_policy
  COMMAND
    "${Python3_EXECUTABLE}" "${PSXPORT_DIR}/tools/check_cpp_style.py"
    --root "${CMAKE_CURRENT_SOURCE_DIR}"
    --compile-commands "${CMAKE_BINARY_DIR}"
  DEPENDS ctr_runtime ctr_port
  USES_TERMINAL
  VERBATIM
)
add_custom_target(
  provision_selftest
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/provision.py" --selftest
  DEPENDS discdump
  USES_TERMINAL
  VERBATIM
)
add_custom_target(
  overlay_extract_selftest
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/extract_overlays.py" --selftest
  USES_TERMINAL
  VERBATIM
)
add_custom_target(
  verify
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/psxport_sync.py"
          --check --resolved "${CMAKE_BINARY_DIR}/psxport_resolved.txt"
  COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" --output-on-failure
  DEPENDS
    cpp_policy
    provision_selftest
    overlay_extract_selftest
    ctr_command_line_test
    ctr_execution_exit_test
    ctr_disc_ownership_test
    ctr_frame_execution_test
    ctr_render_list_diagnostic_test
  USES_TERMINAL
  VERBATIM
)
