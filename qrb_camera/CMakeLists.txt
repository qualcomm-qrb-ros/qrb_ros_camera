cmake_minimum_required(VERSION 3.8.2)
project(qrb_camera VERSION 2.0.3)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(include)

find_package(ament_cmake_auto REQUIRED)

ament_auto_add_library(qrb_camera SHARED
  src/camera_manager.cpp
  src/qcarcam_camera.cpp
  src/qcarcam_stream.cpp
  src/qcarcam_listener.cpp
)

target_link_libraries(qrb_camera
  qcxclient
  qcxosal
  pthread
)

set_target_properties(qrb_camera PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 2)
target_include_directories(qrb_camera PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(BUILD_TESTING)

  ament_auto_add_executable(qrb_camera_test
    test/src/camera_test.cpp
    src/camera_manager.cpp
    src/qcarcam_camera.cpp
    src/qcarcam_stream.cpp
    src/qcarcam_listener.cpp
  )

  target_link_libraries(qrb_camera_test
    qcxclient
    qcxosal
    pthread
    jpeg
  )

  # libqcxosal/libqcxclient have transitive deps (libaosal, libdmabufheap) that
  # are only present at runtime on the target device, not in the build chroot.
  target_link_options(qrb_camera_test PRIVATE "LINKER:--allow-shlib-undefined")

endif()

ament_auto_package()
