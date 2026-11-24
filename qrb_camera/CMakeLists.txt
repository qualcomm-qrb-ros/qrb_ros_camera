# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear
cmake_minimum_required(VERSION 3.16)
project(qrb_camera VERSION 3.0.0 LANGUAGES CXX)

# The camera domain of plugin_discovery. Built with ament_cmake (like the original qrb_camera)
# so that it is an ordinary ROS package for colcon / bloom / meta-ros; the libraries themselves
# link no ROS library:
#   camera_interface           header-only domain interface (ICameraBackend, camera 1.0)
#   qrb_camera                 CameraManager: loads the backends through plugin_discovery;
#                              links no backend and no vendor SDK
#   camera_backend_qmmf        QMMF/CamX backend, a plugin: lib/camera/*.so + manifest
#   camera_backend_qcarcam     QCarCam backend, a plugin: lib/camera/*.so + manifest
#
# Each backend is built when its vendor SDK is found (AUTO, the default), required (ON) or not
# built (OFF):
#   -DQRB_CAMERA_BACKEND_QMMF=AUTO|ON|OFF   -DQRB_CAMERA_BACKEND_QCARCAM=AUTO|ON|OFF
# A package that ships one backend (a Debian binary package, a Yocto PACKAGECONFIG) sets it ON,
# so that a missing SDK fails the build instead of producing an empty package.

find_package(ament_cmake_auto REQUIRED)

include(GNUInstallDirs)
include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(plugin_discovery 0.3 REQUIRED)

# ---------------------------------------------------------------------------------------
# camera_interface: headers only. Backends (ours and customers') link only this.
# ---------------------------------------------------------------------------------------
add_library(camera_interface INTERFACE)
add_library(qrb_camera::camera_interface ALIAS camera_interface)
target_compile_features(camera_interface INTERFACE cxx_std_17)
target_include_directories(camera_interface INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/interface/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(camera_interface INTERFACE plugin_discovery::plugin_discovery)
install(DIRECTORY interface/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# ---------------------------------------------------------------------------------------
# qrb_camera: CameraManager
# ---------------------------------------------------------------------------------------
# Where this prefix installs the camera backends' manifests: the default search directory.
plugin_discovery_plugin_dir(camera QRB_CAMERA_PLUGIN_DIR)

add_library(qrb_camera SHARED src/camera_manager.cpp)
add_library(qrb_camera::qrb_camera ALIAS qrb_camera)
target_include_directories(qrb_camera
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE src)
target_link_libraries(qrb_camera PUBLIC camera_interface plugin_discovery::plugin_discovery)
target_compile_definitions(qrb_camera PRIVATE QRB_CAMERA_PLUGIN_DIR="${QRB_CAMERA_PLUGIN_DIR}")
target_compile_options(qrb_camera PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(qrb_camera PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 3)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# ---------------------------------------------------------------------------------------
# Backends
# ---------------------------------------------------------------------------------------
foreach(b QMMF QCARCAM)
  set(QRB_CAMERA_BACKEND_${b} AUTO CACHE STRING
    "Build the ${b} camera backend: AUTO (when its SDK is found), ON (required), OFF")
  set_property(CACHE QRB_CAMERA_BACKEND_${b} PROPERTY STRINGS AUTO ON OFF)
  if(NOT QRB_CAMERA_BACKEND_${b} MATCHES "^(AUTO|ON|OFF)$")
    message(FATAL_ERROR "QRB_CAMERA_BACKEND_${b} must be AUTO, ON or OFF (is '${QRB_CAMERA_BACKEND_${b}}')")
  endif()
endforeach()

# Reports a backend that cannot be built: an error when it is required, else a warning.
function(qrb_camera_backend_unavailable name option reason)
  if(${option} STREQUAL "ON")
    message(FATAL_ERROR "camera backend '${name}' (${option}=ON): ${reason}")
  endif()
  message(WARNING "camera backend '${name}' skipped: ${reason}")
endfunction()

include(CheckIncludeFileCXX)
set(qrb_camera_backends "")

# --- QMMF -------------------------------------------------------------------------
if(QRB_CAMERA_BACKEND_QMMF STREQUAL "OFF")
  message(STATUS "camera backend 'qmmf' not built (QRB_CAMERA_BACKEND_QMMF=OFF)")
else()
  find_path(QMMF_INCLUDE_DIR qmmf-sdk/qmmf_recorder.h)
  set(qmmf_libs "")
  set(qmmf_missing "")
  foreach(lib camera_metadata qmmf_recorder_client qmmf_camera_metadata qmmf_utils)
    find_library(QMMF_LIB_${lib} ${lib})
    if(QMMF_LIB_${lib})
      list(APPEND qmmf_libs ${QMMF_LIB_${lib}})
    else()
      list(APPEND qmmf_missing ${lib})
    endif()
  endforeach()
  # The original QMMF build used no -I for the SDK: accept a header the compiler finds itself.
  set(qmmf_header_ok FALSE)
  if(QMMF_INCLUDE_DIR)
    set(qmmf_header_ok TRUE)
  else()
    check_include_file_cxx(qmmf-sdk/qmmf_recorder.h QMMF_H_IN_DEFAULT_PATH)
    if(QMMF_H_IN_DEFAULT_PATH)
      set(qmmf_header_ok TRUE)
      set(QMMF_INCLUDE_DIR "")
    endif()
  endif()

  if(NOT qmmf_header_ok OR qmmf_missing)
    qrb_camera_backend_unavailable(qmmf QRB_CAMERA_BACKEND_QMMF
      "QMMF SDK not found (qmmf-sdk/qmmf_recorder.h: ${QMMF_INCLUDE_DIR}; missing libraries: ${qmmf_missing}). If the header is installed elsewhere, pass -DQMMF_INCLUDE_DIR=<dir>")
  else()
    add_library(camera_backend_qmmf SHARED
      backends/qmmf/src/qmmf_backend.cpp
      backends/qmmf/src/qmmf_camera.cpp
      backends/qmmf/src/qmmf_frame.cpp
      backends/qmmf/src/qmmf_stream.cpp)
    target_include_directories(camera_backend_qmmf PRIVATE backends/qmmf/src ${QMMF_INCLUDE_DIR})
    target_link_libraries(camera_backend_qmmf PRIVATE camera_interface ${qmmf_libs})
    target_compile_options(camera_backend_qmmf PRIVATE -Wall -Wextra -Wpedantic)
    plugin_discovery_add_plugin(
      TARGET camera_backend_qmmf
      DOMAIN camera
      NAME camera/QMMFCamera
      CLASS qrb_camera::QMMFBackend
      BASE qrb_camera::ICameraBackend
      BACKEND qmmf
      ORIGIN qcom
      CAPABILITIES unlisted_ids)
    list(APPEND qrb_camera_backends qmmf)
  endif()
endif()

# --- QCarCam ---------------------------------------------------------------------------
if(QRB_CAMERA_BACKEND_QCARCAM STREQUAL "OFF")
  message(STATUS "camera backend 'qcarcam' not built (QRB_CAMERA_BACKEND_QCARCAM=OFF)")
else()
  find_package(Threads REQUIRED)
  find_path(QCARCAM_INCLUDE_DIR qcarcam.h PATH_SUFFIXES qcarcam qcx camera-qcx)
  find_library(QCX_CLIENT_LIB qcxclient)
  find_library(QCX_OSAL_LIB qcxosal)
  # The original QCarCam build just did #include <qcarcam.h> with no -I: accept that too.
  set(qcarcam_header_ok FALSE)
  if(QCARCAM_INCLUDE_DIR)
    set(qcarcam_header_ok TRUE)
  else()
    check_include_file_cxx(qcarcam.h QCARCAM_H_IN_DEFAULT_PATH)
    if(QCARCAM_H_IN_DEFAULT_PATH)
      set(qcarcam_header_ok TRUE)
      set(QCARCAM_INCLUDE_DIR "")
    endif()
  endif()

  if(NOT qcarcam_header_ok OR NOT QCX_CLIENT_LIB OR NOT QCX_OSAL_LIB)
    qrb_camera_backend_unavailable(qcarcam QRB_CAMERA_BACKEND_QCARCAM
      "QCarCam SDK not found (qcarcam.h: ${QCARCAM_INCLUDE_DIR}; qcxclient: ${QCX_CLIENT_LIB}; qcxosal: ${QCX_OSAL_LIB}). If qcarcam.h is installed elsewhere, pass -DQCARCAM_INCLUDE_DIR=<dir>")
  else()
    if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm)")
      message(WARNING "camera backend 'qcarcam': QTimer timestamps need an ARM target "
        "(CMAKE_SYSTEM_PROCESSOR is ${CMAKE_SYSTEM_PROCESSOR})")
    endif()
    add_library(camera_backend_qcarcam SHARED
      backends/qcarcam/src/qcarcam_backend.cpp
      backends/qcarcam/src/qcarcam_camera.cpp
      backends/qcarcam/src/qcarcam_stream.cpp)
    target_include_directories(camera_backend_qcarcam PRIVATE backends/qcarcam/src ${QCARCAM_INCLUDE_DIR})
    target_link_libraries(camera_backend_qcarcam PRIVATE
      camera_interface ${QCX_CLIENT_LIB} ${QCX_OSAL_LIB} Threads::Threads)
    target_compile_options(camera_backend_qcarcam PRIVATE -Wall -Wextra -Wpedantic)
    plugin_discovery_add_plugin(
      TARGET camera_backend_qcarcam
      DOMAIN camera
      NAME camera/QCarCamCamera
      CLASS qrb_camera::QCarCamBackend
      BASE qrb_camera::ICameraBackend
      BACKEND qcarcam
      ORIGIN qcom)
    list(APPEND qrb_camera_backends qcarcam)
  endif()
endif()

if(qrb_camera_backends)
  message(STATUS "camera backends built: ${qrb_camera_backends}")
else()
  message(STATUS "camera backends built: none (no vendor SDK found)")
endif()

# ---------------------------------------------------------------------------------------
# Install / export (the backends are plugins: not exported, found through their manifests)
# ---------------------------------------------------------------------------------------
# Consumers get qrb_camera::qrb_camera and qrb_camera::camera_interface, plus plugin_discovery
# (with plugin_discovery_add_plugin() for backends) through find_package(qrb_camera).
install(TARGETS qrb_camera camera_interface EXPORT qrb_cameraTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
ament_export_targets(qrb_cameraTargets HAS_LIBRARY_TARGET)
ament_export_dependencies(plugin_discovery)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

ament_package()
