/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef QRB_CAMERA__QRB_CAMERA_COMMON_HPP_
#define QRB_CAMERA__QRB_CAMERA_COMMON_HPP_

#include <cstdint>
#include <string>
#include <vector>

namespace qrb_camera
{
enum class StreamType : uint32_t
{
  Track,
  Snapshot,
};

struct CameraFrame
{
  std::string stream_name;
  uint32_t frame_id;
  uint64_t size;
  uint32_t width;
  uint32_t height;
  uint32_t stride;
  uint32_t slice;
  std::string format;
  int64_t timestamp;  // ns
  int32_t fd;
  uint8_t * data;
  int32_t orientation;
};

struct CameraIntrinsicParam
{
  uint32_t width;
  uint32_t height;
  std::string distortion_model;
  std::vector<double> distortion_params;
  double camera_matrix[9];
  double projection_matrix[12];
};

struct CameraExtrinsicParam
{
  double rotation[9];
  double translation[3];
};

struct CameraConfigure
{
  int camera_id;
  std::string camera_type;
  int stream_size;

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
#endif
