/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 *​​​​ ​Changes from Qualcomm Innovation Center, Inc. are provided under the following
 *license: Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef __CAMERA_ROS2_CONFIG_H__
#define __CAMERA_ROS2_CONFIG_H__
#include "qrb_ros_camera/camera_ros2_common.hpp"
#include "rclcpp/rclcpp.hpp"

namespace qrb_ros::camera
{
#define CAMERAROS2_CONFIG_DIRECTORY "/data/misc/ros2/"

typedef struct _CameraRos2CamConfig
{
  uint32_t pipeline_id;
  int32_t camera_id;
  uint32_t width;
  uint32_t height;
  uint32_t stride;
  uint32_t slice;
  std::string format;
  uint8_t fps;
  int32_t publish_freq;
  uint8_t latency_type;
} CameraRos2CamConfig;

class CameraRos2Config
{
public:
  CameraRos2Config(CameraRos2CamConfig * cam_cfg);
  ~CameraRos2Config();
  static CameraRos2Config * create_instance(CameraRos2CamConfig * cam_cfg);
  void destroy();
  CameraRos2CamConfig * get_camera_config() { return &m_cam_cfg; }
  uint32_t get_pipeline_id() { return m_cam_cfg.pipeline_id; }

public:
  int32_t get_camera_id() { return m_cam_cfg.camera_id; };
  void print_config();

private:
  int32_t init(CameraRos2CamConfig * cam_cfg);
  void deinit();

public:
  CameraRos2CamConfig m_cam_cfg;
};
}  // namespace qrb_ros::camera
#endif
