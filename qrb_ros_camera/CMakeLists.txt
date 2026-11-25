# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear
cmake_minimum_required(VERSION 3.8.2)
project(qrb_ros_camera LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -g)
endif()

find_package(ament_cmake_auto REQUIRED)
ament_auto_find_build_dependencies()

# Plain CMake packages (no ROS): linked explicitly through their imported targets.
find_package(qrb_camera 3.0 REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(YAML_CPP REQUIRED yaml-cpp)

include_directories(include)
include_directories(${YAML_CPP_INCLUDE_DIRS})

ament_auto_add_library(camera_node SHARED
  src/camera_node.cpp
  src/configure_parser.cpp
)
target_link_libraries(camera_node
  qrb_camera::qrb_camera
  ${YAML_CPP_LIBRARIES}
)

rclcpp_components_register_nodes(camera_node "qrb_ros::camera::CameraNode")

if(BUILD_TESTING)
  # TestNode: subscribes to a camera topic and prints fps / latency (launch with
  # enable_test_node:=true).
  ament_auto_add_library(${PROJECT_NAME}_test SHARED
    test/src/camera_test.cpp
  )
  rclcpp_components_register_nodes(${PROJECT_NAME}_test
    PLUGIN "qrb_ros::camera::TestNode"
  )
endif()

ament_auto_package(INSTALL_TO_SHARE config launch)
