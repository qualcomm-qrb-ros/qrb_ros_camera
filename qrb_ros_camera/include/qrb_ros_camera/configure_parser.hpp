/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef QRB_ROS_CAMERA__CONFIGURE_PARSER_HPP_
#define QRB_ROS_CAMERA__CONFIGURE_PARSER_HPP_

#include <yaml-cpp/yaml.h>

#include <sensor_msgs/msg/camera_info.hpp>
#include <string>
#include <vector>

#include "camera_interface/camera_types.hpp"
#include "rclcpp/rclcpp.hpp"

namespace qrb_ros
{
namespace camera
{
// Reads the node parameters: camera_id, camera_backend, streams and camera_info_path.
class ConfigureParser
{
public:
  explicit ConfigureParser(rclcpp::Node * node_handler);

  bool init();

  uint32_t get_camera_id();

  // The camera_backend parameter, trimmed and lower-cased; empty: the configured default.
  const std::string & get_camera_backend() const { return camera_backend_; }

  qrb_camera::CameraConfigure & get_camera_param();

  bool get_camera_info(const uint32_t width,
      const uint32_t height,
      sensor_msgs::msg::CameraInfo & camera_info);

private:
  bool load_camera_info_param(std::string & path, qrb_camera::CameraIntrinsicParam & param);

  bool init_{false};
  std::string camera_backend_;
  qrb_camera::CameraConfigure camera_;
  rclcpp::Node * node_handler_;
};
}  // namespace camera
}  // namespace qrb_ros
#endif  // QRB_ROS_CAMERA__CONFIGURE_PARSER_HPP_
