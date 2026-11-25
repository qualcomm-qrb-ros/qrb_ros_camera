/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

// Plain data types of the camera domain, shared by the backends, qrb_camera and its
// consumers (for example qrb_ros_camera). No vendor SDK, no ROS.

#ifndef CAMERA_INTERFACE__CAMERA_TYPES_HPP_
#define CAMERA_INTERFACE__CAMERA_TYPES_HPP_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace qrb_camera
{
enum class StreamType : uint32_t
{
  Track,
  Snapshot,
};

// One frame as handed to the consumer's ImageCallback.
//
// `data` and `fd` point into the backend's buffer and are only valid during the callback.
// QCarCam hands the buffer back to the driver as soon as the callback returns; copy the
// data (or dup() the fd) inside the callback to keep it.
struct CameraFrame
{
  std::string stream_name;
  uint32_t frame_id = 0;
  uint64_t size = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t stride = 0;
  uint32_t slice = 0;
  std::string format;
  int64_t timestamp = 0;  // ns, in the clock of ICameraDevice::now_ns()
  int32_t fd = -1;
  uint8_t * data = nullptr;
  int32_t orientation = 0;
};

using ImageCallback = std::function<void(std::unique_ptr<CameraFrame> frame)>;

struct StreamParam
{
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t frame_rate = 0;
  std::string format;
  int32_t orientation = 0;
  std::string name;
};

struct CameraParam
{
  std::vector<StreamParam> track_params;
  std::vector<StreamParam> snapshot_params;
  std::vector<StreamParam> video_params;
};

struct CameraIntrinsicParam
{
  uint32_t width = 0;
  uint32_t height = 0;
  std::string distortion_model;
  std::vector<double> distortion_params;
  double camera_matrix[9] = {};
  double projection_matrix[12] = {};
};

struct CameraExtrinsicParam
{
  double rotation[9] = {};
  double translation[3] = {};
};

struct CameraConfigure
{
  int camera_id = -1;
  std::string camera_type;
  int stream_size = 0;

  std::vector<std::string> stream_names;
  std::vector<std::string> stream_formats;
  std::vector<uint32_t> stream_widths;
  std::vector<uint32_t> stream_heights;
  std::vector<uint32_t> stream_fps;
  std::vector<StreamType> stream_type;

  std::vector<CameraIntrinsicParam> camera_intrinsic_params;
  CameraExtrinsicParam camera_extrinsic_param;
};
}  // namespace qrb_camera

#endif  // CAMERA_INTERFACE__CAMERA_TYPES_HPP_
