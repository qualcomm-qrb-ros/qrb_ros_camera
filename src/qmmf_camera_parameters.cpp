/*
 * Copyright (c) 2015, 2017, The Linux Foundation. All rights reserved.
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
 *
 */
/*
 *​​​​ ​Changes from Qualcomm Innovation Center, Inc. are provided under the following
 *license: Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include "qrb_ros_camera/qmmf_camera_parameters.hpp"

#include "qrb_ros_camera/qmmf_camera.hpp"
#include "qrb_ros_camera/qmmf_camera_common.hpp"
namespace qrb_ros::camera
{
/* helper function to cast the private params to CameraParameters */
inline static QMMFCameraParams * params_cast(void * priv)
{
  return static_cast<QMMFCameraParams *>(priv);
}

CameraParams::CameraParams()
{
  priv_ = nullptr;
}

int CameraParams::init(ICameraDevice * device)
{
  int rc = EXIT_SUCCESS;
  if (device == nullptr) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "nullptr device provided");
    rc = EXIT_FAILURE;
    goto bail;
  }
  priv_ = device->get_parameters();
  if (priv_ == nullptr) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "Params init failed");
    rc = EXIT_FAILURE;
    goto bail;
  }
  device_ = device;
bail:
  return rc;
}

CameraParams::~CameraParams() {}

std::vector<ImageSize> CameraParams::get_supported_preview_sizes() const
{
  std::vector<ImageSize> img_sizes;
  uint32_t width, height;
  camera_metadata_entry_t entry;
  params_cast(priv_)->mutex_lock();
  if ((params_cast(priv_)->static_default_meta_info.exists(
          ANDROID_SCALER_AVAILABLE_PROCESSED_SIZES))) {
    entry =
        params_cast(priv_)->static_default_meta_info.find(ANDROID_SCALER_AVAILABLE_PROCESSED_SIZES);
    for (uint32_t i = 0; i < entry.count; i += 2) {
      width = entry.data.i32[i + 0];
      height = entry.data.i32[i + 1];
      img_sizes.emplace_back(width, height);
    }
  } else {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"),
        "ANDROID_SCALER_AVAILABLE_PROCESSED_SIZES does not exist");
  }
  params_cast(priv_)->mutex_unlock();
  return img_sizes;
}

void CameraParams::set_preview_size(const ImageSize & size)
{
  params_cast(priv_)->mutex_lock();
  params_cast(priv_)->track_params_ptr[CAMERA_QMMF_TRACK_PREVIEW]->width = size.width;
  params_cast(priv_)->track_params_ptr[CAMERA_QMMF_TRACK_PREVIEW]->height = size.height;
  params_cast(priv_)->mutex_unlock();
}

ImageSize CameraParams::get_preview_size() const
{
  ImageSize size;
  size.width = params_cast(priv_)->track_params_ptr[CAMERA_QMMF_TRACK_PREVIEW]->width;
  size.height = params_cast(priv_)->track_params_ptr[CAMERA_QMMF_TRACK_PREVIEW]->height;
  return size;
}

int CameraParams::commit()
{
  // set the current state of paramters in camera device
  return device_->set_parameters(*this);
}

std::string CameraParams::get(const std::string & key) const
{
  std::string str;
  RCLCPP_DEBUG(rclcpp::get_logger("qrb_ros_camera"), "API is deprecated for this target \n");
  return str;
}

void CameraParams::set(const std::string & key, const std::string & value)
{
  return;
}

std::vector<Range> CameraParams::get_supported_preview_fps_ranges() const
{
  std::vector<Range> ranges;
  camera_metadata_entry_t entry;
  params_cast(priv_)->mutex_lock();
  if ((params_cast(priv_)->static_default_meta_info.exists(
          ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES))) {
    entry = params_cast(priv_)->static_default_meta_info.find(
        ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
    for (size_t i = 0; i < entry.count; i += 2) {
      /* Multiply Fps * 1000 for backward compatibility of HAL1 */
      ranges.push_back(Range((entry.data.i32[i] * 1000), (entry.data.i32[i + 1] * 1000), 1));
    }
  } else {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"),
        "ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES does not exist");
  }
  std::vector<Range> ranges2 = get_HFRFps_range();
  ranges.insert(ranges.end(), ranges2.begin(), ranges2.end());
  params_cast(priv_)->mutex_unlock();
  return ranges;
}
Range CameraParams::get_preview_fps_range() const
{
  Range range;
  camera_metadata_entry_t entry;
  params_cast(priv_)->mutex_lock();
  if ((params_cast(priv_)->static_default_meta_info.exists(ANDROID_CONTROL_AE_TARGET_FPS_RANGE))) {
    entry = params_cast(priv_)->static_default_meta_info.find(ANDROID_CONTROL_AE_TARGET_FPS_RANGE);
    if (entry.count >= 2) {
      range.min =
          entry.data.i32[0] * 1000; /* Multiply Fps * 1000 for backward compatibility of HAL1 */
      range.max = entry.data.i32[0 + 1] * 1000;
      range.step = 1;
    } else {
      RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), " Invalid Fps range");
    }
  } else {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"),
        " ANDROID_CONTROL_AE_TARGET_FPS_RANGE does not exist");
  }
  params_cast(priv_)->mutex_unlock();
  return range;
}

std::vector<Range> CameraParams::get_HFRFps_range() const
{
  std::vector<Range> ranges;
  uint32_t last_fps = 0;
  uint32_t width_offset = 0;
  uint32_t height_offset = 1;
  uint32_t min_fps_offset = 2;
  uint32_t max_fps_offset = 3;
  uint32_t batch_size_offset = 4;
  uint32_t hfr_size = 5;
  bool hfr_supported_ = false;
  camera_metadata_entry meta_entry =
      params_cast(priv_)->static_default_meta_info.find(ANDROID_REQUEST_AVAILABLE_CAPABILITIES);
  for (uint32_t i = 0; i < meta_entry.count; ++i) {
    uint8_t caps = meta_entry.data.u8[i];
    if (ANDROID_REQUEST_AVAILABLE_CAPABILITIES_CONSTRAINED_HIGH_SPEED_VIDEO == caps) {
      hfr_supported_ = true;
      break;
    }
  }
  if (!hfr_supported_) {
    return ranges;
  }
  meta_entry = params_cast(priv_)->static_default_meta_info.find(
      ANDROID_CONTROL_AVAILABLE_HIGH_SPEED_VIDEO_CONFIGURATIONS);
  for (uint32_t i = 0; i < meta_entry.count; i += hfr_size) {
    uint32_t width = meta_entry.data.i32[i + width_offset];
    uint32_t height = meta_entry.data.i32[i + height_offset];
    uint32_t min_fps = meta_entry.data.i32[i + min_fps_offset];
    uint32_t max_fps = meta_entry.data.i32[i + max_fps_offset];
    uint32_t batch = meta_entry.data.i32[i + batch_size_offset];
    if (min_fps == max_fps) {  // Only constant framerates are supported
      if (last_fps != min_fps) {
        ranges.push_back(Range(min_fps * 1000, max_fps * 1000, 1));
        last_fps = min_fps;
      }
    }
  }
  return ranges;
}

void CameraParams::set_preview_fps_range(const Range & value)
{
  bool success = false;
  camera_metadata_entry_t entry;
  params_cast(priv_)->mutex_lock();
  if ((params_cast(priv_)->static_default_meta_info.exists(ANDROID_CONTROL_AE_TARGET_FPS_RANGE))) {
    entry = params_cast(priv_)->static_default_meta_info.find(ANDROID_CONTROL_AE_TARGET_FPS_RANGE);
    size_t i = 0;
    for (i = 0; i < entry.count; i += 2) {
      if (value.min / 1000 == entry.data.i32[i] && value.max / 1000 == entry.data.i32[i + 1]) {
        params_cast(priv_)->track_params_ptr[CAMERA_QMMF_TRACK_PREVIEW]->framerate =
            value.max / 1000;
        RCLCPP_INFO(rclcpp::get_logger("qrb_ros_camera"), "Setting  fps range = %d %d ",
            entry.data.i32[i], entry.data.i32[i + 1]);
        success = true;
        break;
      }
    }
    if (i >= entry.count) {
      RCLCPP_DEBUG(
          rclcpp::get_logger("qrb_ros_camera"), " Error: preview range not valid for regular fps");
    }
  } else {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"),
        " ANDROID_CONTROL_AE_TARGET_FPS_RANGE does not exist");
  }
  if (!success) {
    std::vector<Range> ranges = get_HFRFps_range();
    for (size_t i = 0; i < ranges.size(); i++) {
      if ((ranges[i].min == value.min) && (ranges[i].max == value.max)) {
        success = true;
        params_cast(priv_)->track_params_ptr[CAMERA_QMMF_TRACK_PREVIEW]->framerate =
            value.max / 1000;
        RCLCPP_INFO(rclcpp::get_logger("qrb_ros_camera"), "Setting  fps(HFR) range = %d %d ",
            value.max / 1000, value.max / 1000);
        break;
      }
    }
  }
  params_cast(priv_)->track_params_ptr[CAMERA_QMMF_TRACK_PREVIEW]->framerate = value.max;
  params_cast(priv_)->mutex_unlock();
  return;
}

std::string CameraParams::to_string() const
{
  std::string temp_return;
  RCLCPP_DEBUG(rclcpp::get_logger("qrb_ros_camera"), "API is deprecated for this target \n");
  return temp_return;
}

std::vector<std::string> CameraParams::get_supported_preview_formats() const
{
  std::vector<std::string> formats;
  params_cast(priv_)->mutex_lock();
  camera_metadata_entry_t entry;
  if (params_cast(priv_)->static_default_meta_info.exists(ANDROID_SCALER_AVAILABLE_FORMATS)) {
    entry = params_cast(priv_)->static_default_meta_info.find(ANDROID_SCALER_AVAILABLE_FORMATS);
    for (uint32_t i = 0; i < entry.count; i++) {
      /* Check only format supported by qmmf-sdk */
      if (entry.data.i32[i] == ANDROID_SCALER_AVAILABLE_FORMATS_YCbCr_420_888) {
        formats.emplace_back(FORMAT_NV12_UBWC);
      }
    }
  } else {
    RCLCPP_ERROR(
        rclcpp::get_logger("qrb_ros_camera"), "ANDROID_SCALER_AVAILABLE_FORMATS does not exist");
  }
  params_cast(priv_)->mutex_unlock();
  return formats;
}

std::string CameraParams::get_preview_format() const
{
  std::string format;
  if (params_cast(priv_)->track_params_ptr[CAMERA_QMMF_TRACK_PREVIEW]->format ==
      VideoFormat::kBayerRDI12BIT) {
    format = FORMAT_RAW12;
  } else if (params_cast(priv_)->track_params_ptr[CAMERA_QMMF_TRACK_PREVIEW]->format ==
             VideoFormat::kBayerRDI10BIT) {
    format = FORMAT_RAW10;
  } else if (params_cast(priv_)->track_params_ptr[CAMERA_QMMF_TRACK_PREVIEW]->format ==
             VideoFormat::kNV12UBWC) {
    format = FORMAT_NV12_UBWC;
  } else if (params_cast(priv_)->track_params_ptr[CAMERA_QMMF_TRACK_PREVIEW]->format ==
             VideoFormat::kNV12) {
    format = FORMAT_NV12;
  } else {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "Error : format not in list \n");
  }
  return format;
}

void CameraParams::set_preview_format(const std::string & value)
{
  params_cast(priv_)->mutex_lock();
  if (FORMAT_RAW12 == value) {
    params_cast(priv_)->track_params_ptr[CAMERA_QMMF_TRACK_PREVIEW]->format =
        VideoFormat::kBayerRDI12BIT;
  } else if (FORMAT_RAW10 == value) {
    params_cast(priv_)->track_params_ptr[CAMERA_QMMF_TRACK_PREVIEW]->format =
        VideoFormat::kBayerRDI10BIT;
  } else if (FORMAT_NV12_UBWC == value) {
    params_cast(priv_)->track_params_ptr[CAMERA_QMMF_TRACK_PREVIEW]->format =
        VideoFormat::kNV12UBWC;
  } else if (FORMAT_NV12 == value) {
    params_cast(priv_)->track_params_ptr[CAMERA_QMMF_TRACK_PREVIEW]->format = VideoFormat::kNV12;
  } else {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "Error : unsupport format \n");
  }
  params_cast(priv_)->mutex_unlock();
  return;
}
}  // namespace qrb_ros::camera