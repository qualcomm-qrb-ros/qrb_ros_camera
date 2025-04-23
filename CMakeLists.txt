cmake_minimum_required(VERSION 3.8.2)
project(qrb_ros_camera
        LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Default to C99
if (NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif ()
# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -g)
endif()

include_directories (./include)

include_directories (${SYSROOT_INCDIR}/qmmf-sdk/common/utils PRIVATE)
include_directories (${SYSROOT_INCDIR}/qmmf-sdk/include PRIVATE)
include_directories (${SYSROOT_INCDIR}/qmmf-sdk PRIVATE)
include_directories (${SYSROOT_INCDIR}/opencv4)
include_directories (${SYSROOT_INCDIR}/camx-api/camx/service)

find_package(ament_cmake_auto REQUIRED)
ament_auto_find_build_dependencies()

ament_auto_add_executable(qrb_ros_camera src/main.cpp)

ament_auto_add_library(camera_component SHARED
  src/camera_ros2_config.cpp
  src/camera_ros2_node.cpp
  src/qmmf_camera_parameters.cpp
  src/qmmf_camera.cpp
  src/qmmf_ros2_pipeline.cpp
  )

  ament_auto_add_library(test_component SHARED
  src/camera_ros2_config.cpp
  src/test_ros2_node.cpp
  src/test_ros2_out_node.cpp)

rclcpp_components_register_nodes(camera_component "qrb_ros::camera::CameraNode")
rclcpp_components_register_nodes(test_component "qrb_ros::camera::TestNode")
rclcpp_components_register_nodes(test_component "qrb_ros::camera::TestOutNode")

target_link_libraries(camera_component dl pthread camera_metadata)

target_link_libraries( camera_component
  camera_metadata
  qmmf_recorder_client
  qmmf_utils
  yaml-cpp
)

target_link_libraries(qrb_ros_camera
  camera_component
  test_component
)

ament_auto_package(INSTALL_TO_SHARE config launch)