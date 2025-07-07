/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef QRB_ROS_CAMERA__CONFIGURE_PARSER_HPP_
#define QRB_ROS_CAMERA__CONFIGURE_PARSER_HPP_

#include <yaml-cpp/yaml.h>

#include <sensor_msgs/msg/camera_info.hpp>
#include <string>
#include <vector>

#include "qrb_camera/qrb_camera_common.hpp"
#include "rclcpp/rclcpp.hpp"

#define CAMERAINFO_DIR "/var/cache/camera"

namespace qrb_ros
{
namespace camera
{
class ConfigureParser
{
public:
  explicit ConfigureParser(rclcpp::Node * node_handler);

  uint32_t get_camera_id();

  qrb_camera::CameraConfigure & get_camera_param();

  bool init();

  bool get_camera_info(const uint32_t width,
      const uint32_t height,
      sensor_msgs::msg::CameraInfo & camera_info);

private:
  bool load_camera_info_param(std::string & path, qrb_camera::CameraIntrinsicParam & param);

  bool init_{ false };
  qrb_camera::CameraConfigure camera_;
  rclcpp::Node * node_handler_;
};
}  // namespace camera
}  // namespace qrb_ros
#endif