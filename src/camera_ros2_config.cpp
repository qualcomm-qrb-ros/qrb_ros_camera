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
#include "qrb_ros_camera/camera_ros2_config.hpp"
namespace qrb_ros::camera
{
CameraRos2Config::CameraRos2Config(CameraRos2CamConfig * cam_cfg) {}

CameraRos2Config::~CameraRos2Config()
{
  deinit();
}

CameraRos2Config * CameraRos2Config::create_instance(CameraRos2CamConfig * cam_cfg)
{
  // CAMERAROS2_INFO("Enter");
  CameraRos2Config * cfg = nullptr;
  if (cfg == nullptr) {
    cfg = new CameraRos2Config(cam_cfg);
  }
  if (cfg == nullptr) {
    return cfg;
  }
  if (cfg->init(cam_cfg)) {
    cfg->destroy();
    cfg = nullptr;
  }
  return cfg;
}

void CameraRos2Config::destroy()
{
  delete this;
}

int32_t CameraRos2Config::init(CameraRos2CamConfig * cam_cfg)
{
  if (cam_cfg == nullptr) {
    return -EINVAL;
  }
  m_cam_cfg.pipeline_id = cam_cfg->pipeline_id;
  m_cam_cfg.camera_id = cam_cfg->camera_id;
  m_cam_cfg.width = cam_cfg->width;
  m_cam_cfg.height = cam_cfg->height;
  m_cam_cfg.stride = cam_cfg->stride;
  m_cam_cfg.slice = cam_cfg->slice;
  m_cam_cfg.format = cam_cfg->format;
  m_cam_cfg.fps = cam_cfg->fps;
  m_cam_cfg.publish_freq = cam_cfg->publish_freq;
  m_cam_cfg.latency_type = cam_cfg->latency_type;
  return 0;
}

void CameraRos2Config::deinit() {}

void CameraRos2Config::print_config()
{
  RCLCPP_INFO(rclcpp::get_logger("qrb_ros_camera"),
      "camera parameters..."
      "pipeline_id:   %u \n"
      "camera_id:     %d \n"
      "width:        %u \n"
      "height:       %u \n"
      "stride:       %u \n"
      "slice:        %u \n"
      "format:       %s \n"
      "fps:          %d \n"
      "publish_freq:  %d \n"
      "latency_type:  %d \n",
      m_cam_cfg.pipeline_id, m_cam_cfg.camera_id, m_cam_cfg.width, m_cam_cfg.height,
      m_cam_cfg.stride, m_cam_cfg.slice, m_cam_cfg.format.c_str(), m_cam_cfg.fps,
      m_cam_cfg.publish_freq, m_cam_cfg.latency_type);
}
}  // namespace qrb_ros::camera
