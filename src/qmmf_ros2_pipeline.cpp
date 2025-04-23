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
#include "qrb_ros_camera/qmmf_ros2_pipeline.hpp"

#include "qrb_ros_camera/camera_ros2_common.hpp"
#include "qrb_ros_camera/qmmf_camera.hpp"
#include "qrb_ros_camera/qmmf_camera_parameters.hpp"
namespace qrb_ros::camera
{
ImageSize DEFAULT_SIZE(1920, 1080);
#define DEFAULT_CAMERA_FPS 30

QmmfRos2Pipeline::QmmfRos2Pipeline() : camera_(nullptr), m_p_config_(nullptr), m_base_time_(0) {}

QmmfRos2Pipeline::QmmfRos2Pipeline(CameraRos2Config * cfg)
  : camera_(nullptr), m_p_config_(cfg), m_base_time_(0)
{
}

QmmfRos2Pipeline::~QmmfRos2Pipeline() {}

QmmfRos2Pipeline * QmmfRos2Pipeline::create_instance(CameraRos2Config * cfg)
{
  QmmfRos2Pipeline * qmmf_ros2_pipeline = new QmmfRos2Pipeline(cfg);
  if (qmmf_ros2_pipeline == nullptr) {
    return qmmf_ros2_pipeline;
  }
  if (qmmf_ros2_pipeline->init(cfg)) {
    qmmf_ros2_pipeline->destroy();
    qmmf_ros2_pipeline = nullptr;
  }
  return qmmf_ros2_pipeline;
}

void QmmfRos2Pipeline::destroy()
{
  stop();
  deinit();
  delete this;
}

int32_t QmmfRos2Pipeline::init(CameraRos2Config * cfg)
{
  if (cfg == nullptr) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "Error: set cfg first!");
    return -EINVAL;
  }
  // this->set_default_config(); // set default TestConfig structure
  int rc = EXIT_FAILURE;
  rc = ICameraDevice::create_instance(cfg->get_camera_id(), &camera_);
  if (rc != 0) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "Could not open camera %d, rc: %d\n",
        cfg->get_camera_id(), rc);
    return rc;
  }
  camera_->add_listener(this);
  rc = params_.init(camera_);
  if (rc != 0) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "failed to init parameters\n");
    ICameraDevice::delete_instance(&camera_);
    return rc;
  }
  this->set_parameters();
  rc = this->start();
  if (rc != 0) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "Pipeline start failed. ret: %d\n", rc);
    return rc;
  }
  RCLCPP_INFO(rclcpp::get_logger("qrb_ros_camera"), "Pipeline start successful. ret: %d\n", rc);
  return 0;
}

// convert CameraRos2CamConfig to CameraParams
int QmmfRos2Pipeline::set_parameters()
{
  CameraRos2CamConfig * cam_cfg = m_p_config_->get_camera_config();
  std::string output_format = FORMAT_NV12;
  p_size_ = qrb_ros::camera::ImageSize((int)cam_cfg->width, (int)cam_cfg->height);
  int rc = this->parse_format(output_format);
  if (rc) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "parse_format Error\n");
    // return EXIT_FAILURE;
  }
  params_.set_preview_size(p_size_);
  params_.set_preview_format(output_format);
  params_.set_preview_fps_range(Range((int)cam_cfg->fps, (int)cam_cfg->fps, 0));
  return params_.commit();
}

int QmmfRos2Pipeline::parse_format(string & previewFormat)
{
  CameraRos2CamConfig * cam_cfg = m_p_config_->get_camera_config();
  // default value
  previewFormat = FORMAT_NV12;
  // RAW format
  if (cam_cfg->format.find("RAW") != cam_cfg->format.npos) {
    int32_t bit_width = 0;
    char raw_format[16];
    sscanf(cam_cfg->format.c_str(), "RAW%d/%s", &bit_width, raw_format);
    if (bit_width <= 0) {
      RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "Invalid raw_format");
      return EXIT_FAILURE;
    }
    if (strcmp(raw_format, "bggr") && strcmp(raw_format, "rggb") && strcmp(raw_format, "gbrg") &&
        strcmp(raw_format, "grbg") && strcmp(raw_format, "mono")) {
      RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "Invalid raw_format for filter");
      return EXIT_FAILURE;
    }
    if (bit_width == 10) {
      previewFormat = FORMAT_RAW10;
      return EXIT_SUCCESS;
    } else if (bit_width == 12) {
      previewFormat = FORMAT_RAW12;
      return EXIT_SUCCESS;
    } else {
      RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "RAW bit width not support");
      return EXIT_FAILURE;
    }
  } else {
    /// Non RAW format
    const char * format;
    if (cam_cfg->format == "mono8" || cam_cfg->format == "rgb8" || cam_cfg->format == "nv12") {
      previewFormat = FORMAT_NV12;
      return EXIT_SUCCESS;
    } else {
      RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "format not support");
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}

void QmmfRos2Pipeline::deinit()
{
  camera_->remove_listener(this);
  ICameraDevice::delete_instance(&camera_);
}

int32_t QmmfRos2Pipeline::start()
{
  int32_t result = camera_->start_preview();
  return result;
}

void QmmfRos2Pipeline::stop()
{
  camera_->stop_preview();
  return;
}

void QmmfRos2Pipeline::on_preview_frame(ICameraFrame * buffer)
{
  CameraRos2Frame frame;
  memset(&frame, 0, sizeof(CameraRos2Frame));
  CameraRos2FrameInfo * frameInfo = &frame.info;
  CameraRos2CamConfig * cam_cfg = m_p_config_->get_camera_config();
  frame.data = buffer->data;
  frameInfo->pipeline_id = cam_cfg->pipeline_id;
  frameInfo->camera_id = cam_cfg->camera_id;
  frameInfo->frame_id = m_cur_frame_id_;
  frameInfo->width = cam_cfg->width;
  frameInfo->height = cam_cfg->height;
  frameInfo->stride = buffer->stride;
  frameInfo->slice = buffer->slice;
  frameInfo->format = cam_cfg->format;
  frameInfo->size = (uint64_t)buffer->size;
  frameInfo->timestamp = (int64_t)(buffer->timestamp + m_base_time_);
  frameInfo->latency_type = cam_cfg->latency_type;
  frameInfo->latency = 0;
  frameInfo->fd = buffer->fd;
  if (m_p_publish_ && cam_cfg->publish_freq > 0 && m_cur_frame_id_ % cam_cfg->publish_freq == 0) {
    // publish buffer
    m_p_publish_(&frame);
  }
  m_cur_frame_id_++;
  return;
}

void QmmfRos2Pipeline::on_video_frame(ICameraFrame * frame)
{
  return;
}

void QmmfRos2Pipeline::on_picture_frame(ICameraFrame * frame)
{
  return;
}

void QmmfRos2Pipeline::on_error()
{
  RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "Frame Error!");
  return;
}

void QmmfRos2Pipeline::register_publish(CameraRos2MsgPublishFrameFunc publish)
{
  m_p_publish_ = publish;
}
}  // namespace qrb_ros::camera
