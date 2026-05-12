cmake_minimum_required(VERSION 3.10)
project(qrb_camera VERSION 2.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(include)


set(BACKEND_SOURCES
  src/camera_manager.cpp
  src/qcarcam_camera.cpp
  src/qcarcam_stream.cpp
  src/qcarcam_listener.cpp
  src/qcarcam_frame.cpp
  src/atrace_stub.cpp 
)

find_library(QCXCLIENT_LIB qcxclient REQUIRED)
find_library(QCXOSAL_LIB   qcxosal)

message(STATUS "qcxclient : ${QCXCLIENT_LIB}")
message(STATUS "qcxosal   : ${QCXOSAL_LIB}")

set(BACKEND_LIBS
  ${QCXCLIENT_LIB}
  pthread
)
if(QCXOSAL_LIB)
  list(APPEND BACKEND_LIBS ${QCXOSAL_LIB})
endif()


# ---------------------------------------------------------------------------
# Shared library: libqrb_camera.so
# ---------------------------------------------------------------------------
add_library(qrb_camera SHARED ${BACKEND_SOURCES})

target_link_libraries(qrb_camera ${BACKEND_LIBS})

set_target_properties(qrb_camera PROPERTIES
  VERSION   ${PROJECT_VERSION}
  SOVERSION 2
)

target_include_directories(qrb_camera PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ---------------------------------------------------------------------------
# Test executables
# ---------------------------------------------------------------------------
add_executable(camera_test
  test/src/camera_test.cpp
  ${BACKEND_SOURCES}
)
target_compile_definitions(camera_test PRIVATE USE_QCARCAM)
target_include_directories(camera_test PRIVATE
  include
)
target_link_libraries(camera_test ${BACKEND_LIBS} jpeg)

