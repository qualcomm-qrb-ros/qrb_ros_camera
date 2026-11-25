# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear

# CameraManager tests on fake backends. Nothing here is installed.

set(fake_backend_dir ${CMAKE_CURRENT_BINARY_DIR}/fake_backends)

add_library(qrb_camera_fake_events SHARED fake_events.cpp)

# fake_backend(<class> <backend name> <default camera ids> [extra compile definitions...])
set(fake_backend_targets "")
function(fake_backend cls backend devices)
  set(target fake_backend_${cls})
  string(TOUPPER "${backend}" upper)
  add_library(${target} SHARED fake_backend.cpp)
  target_compile_definitions(${target} PRIVATE
    FAKE_CLASS=${cls} "FAKE_BACKEND=\"${backend}\"" "FAKE_UPPER=\"${upper}\""
    "FAKE_DEVICES=\"${devices}\"" ${ARGN})
  target_link_libraries(${target} PRIVATE
    camera_interface plugin_discovery::plugin_discovery qrb_camera_fake_events)
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  set_target_properties(${target} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${fake_backend_dir})
  set(fake_backend_targets ${fake_backend_targets} ${target} PARENT_SCOPE)
endfunction()

fake_backend(FakeA fake_a "0,1")
fake_backend(FakeB fake_b "5")
fake_backend(FakeAny fake_any "0")
# Built with another ABI tag: plugin_discovery must refuse it before creating any object.
fake_backend(FakeAbi fake_abi "0" "PLUGIN_DISCOVERY_ABI_TAG=\"fake-abi\"")

add_executable(camera_manager_test camera_manager_test.cpp)
target_link_libraries(camera_manager_test PRIVATE qrb_camera qrb_camera_fake_events)
target_compile_definitions(camera_manager_test PRIVATE FAKE_BACKEND_DIR="${fake_backend_dir}")
target_compile_options(camera_manager_test PRIVATE -Wall -Wextra -Wpedantic)
add_dependencies(camera_manager_test ${fake_backend_targets})

add_test(NAME camera_manager_test COMMAND camera_manager_test)
