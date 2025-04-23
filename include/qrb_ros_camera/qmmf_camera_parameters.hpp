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
#ifndef __QMMF_CAMERA_PARAMETERS_H__
#define __QMMF_CAMERA_PARAMETERS_H__

#include "qrb_ros_camera/qmmf_camera.hpp"
#include "rclcpp/rclcpp.hpp"

namespace qrb_ros::camera
{
/**
 * Image frame dimensions
 */
struct ImageSize
{
  int width;  /*!< Image width in pixels */
  int height; /*!< Image height in pixels */

  ImageSize() : width(0), height(0) {}
  ImageSize(int w, int h) : width(w), height(h) {}
};

/**
 * Structure for storing values for ranged parameters such as
 * brightness, contrast, fps etc.
 */
struct Range
{
  int min;  /*!< minimum value */
  int max;  /*!< maximum value */
  int step; /*!< step increment for intermediate values */

  Range() : min(0), max(0), step(0) {}
  Range(int m, int x, int s) : min(m), max(x), step(s) {}
};

const std::string FORMAT_NV12 = "nv12";
const std::string FORMAT_NV12_UBWC = "ubwc";
const std::string FORMAT_NV21 = "yuv420sp";
const std::string FORMAT_RAW10 = "raw10";
const std::string FORMAT_RAW12 = "raw12";
const std::string FORMAT_JPEG = "jpeg";

class CameraParams
{
public:
  CameraParams();

  virtual ~CameraParams();

  /**
   * initialize the object by getting current state of parameters
   * from device.
   *
   * @param device : A valid camera device object
   *
   * @return int : 0 on success
   */
  virtual int init(ICameraDevice * device);

  /**
   * get a string representation of the object
   *
   * @return std::string
   */
  std::string to_string() const;

  /**
   * Updates the current state of the parameters to camera device.
   * Fails for any invalid entries.
   *
   * @return int : 0 on success
   */
  virtual int commit();

  /**
   * get preview sizes supported by the camera
   *
   * @return std::vector<ImageSize> : list of preview sizes
   */
  std::vector<ImageSize> get_supported_preview_sizes() const;

  /**
   * set preview size
   *
   * @param size
   */
  void set_preview_size(const ImageSize & size);

  /**
   * get current preview size
   *
   * @return ImageSize
   */
  ImageSize get_preview_size() const;

  /**
   * get video sizes supported by the camera
   *
   * @return std::vector<ImageSize> : list of video sizes
   */
  std::vector<ImageSize> get_supported_video_sizes() const;

  /**
   * get current video size
   *
   * @return ImageSize
   */
  ImageSize get_video_size() const;

  /**
   * set video size
   *
   * @param size
   */
  void set_video_size(const ImageSize & size);

  /**
   * get picture sizes supported by the camera
   *
   * @return std::vector<ImageSize> : list of picture sizes
   */
  std::vector<ImageSize> get_supported_picture_sizes(std::string format = FORMAT_JPEG) const;

  /**
   * get current picture size
   *
   * @return ImageSize
   */
  ImageSize get_picture_size() const;

  /**
   * set picture size
   *
   * see @ref takePicture
   *
   * @param size
   */
  void set_picture_size(const ImageSize & size);

  /**
   * get picture thumbnail size
   *
   * @return ImageSize
   */
  ImageSize get_picture_thumb_nail_size() const;

  /**
   * set picture thumbnail size
   *
   * @param size
   */
  void set_picture_thumb_nail_size(const ImageSize & size);

  /**
   * generic get function to get string representation of value of
   * a parameter using a key.
   *
   * @param key [in]
   *
   * @return std::string : value
   */
  virtual std::string get(const std::string & key) const;

  /**
   * generic set function to set value of a parameter using
   * key-value pair
   *
   * @param key [in]
   * @param value [in]
   */
  virtual void set(const std::string & key, const std::string & value);

  /**
   * get a list of supported focus modes
   *
   * @return vector<string> : focus mode values
   */
  std::vector<std::string> get_supported_focus_modes() const;

  /**
   * get current value of focus mode
   *
   * @return std::string
   */
  std::string get_focus_mode() const;

  /**
   * set focus mode value
   *
   * @param value
   */
  void set_focus_mode(const std::string & value);

  /**
   * get a list of supported whitebalance modes
   *
   * @return vector<string> : whitebalance values
   */
  std::vector<std::string> get_supported_white_balance() const;

  /**
   * get current value of whitebalance mode
   *
   * @return std::string
   */
  std::string get_white_balance() const;

  /**
   * set whitebalance mode value
   *
   * @param value
   */
  void set_white_balance(const std::string & value);

  /**
   * get a list of supported ISO modes
   *
   * @return vector<string> : ISO values
   */
  std::vector<std::string> get_supported_ISO() const;

  /**
   * get current value of ISO mode
   *
   * @return std::string
   */
  std::string get_ISO() const;

  /**
   * set ISO mode value
   *
   * @param value
   */
  void set_ISO(const std::string & value);

  /**
   * get a list of supported Sharpness modes
   *
   * (HAL3 only)
   *
   * @return vector<string> : Sharpness values
   */
  std::vector<std::string> get_supported_sharpness_mode() const;

  /**
   * get current value of Sharpness mode
   *
   * (HAL3 only)
   *
   * @return std::string
   */
  std::string get_sharpness_mode() const;

  /**
   * set Sharpness mode value
   *
   * (HAL3 only)
   *
   * @param value
   */
  void set_sharpness_mode(const std::string & value);

  /**
   * get a list of supported ToneMap modes
   *
   * (HAL3 only)
   *
   * @return vector<string> : Sharpness values
   */
  std::vector<std::string> get_supported_tone_map_mode() const;

  /**
   * get current value of ToneMap mode
   *
   * (HAL3 only)
   *
   * @return std::string
   */
  std::string get_tone_map_mode() const;

  /**
   * set ToneMap mode value
   *
   * (HAL3 only)
   *
   * @param value
   */
  void set_tone_map_mode(const std::string & value);

  /**
   * get a range of supported sharpness values
   *
   *(HAL3 only)
   *
   * @return Range : sharpness range
   */
  Range get_supported_sharpness() const;

  /**
   * get current sharpness value
   *
   * @return int
   */
  int get_sharpness() const;

  /**
   * set sharpness value
   *
   * @param value
   */
  void set_sharpness(int value);

  /**
   * get a range of supported brightness values
   *
   * @return Range : brightness range
   */
  Range get_supported_brightness() const;

  /**
   * get current brightness value
   *
   * @return int
   */
  int get_brightness() const;

  /**
   * set brightness value
   *
   * @param value
   */
  void set_brightness(int value);

  /**
   * get a range of supported contrast values
   *
   * @return Range : contrast range
   */
  Range get_supported_contrast() const;

  /**
   * get current contrast value
   *
   * @return int
   */
  int get_contrast() const;

  /**
   * set contrast value
   *
   * @param value
   */
  void set_contrast(int value);

  /**
   * get supported ranges for preview Fps The Fps range has valid
   * min and max value. Actual fixed point Fps value is calculated
   * by dividing the min and max values by 1000. For example, max
   * value of 26123 represents 26.123 fps.
   *
   * @return vector<Range> : preview fps ranges
   */
  std::vector<Range> get_supported_preview_fps_ranges() const;

  /**
   * get current preview fps range value
   *
   * @return Range
   */
  Range get_preview_fps_range() const;

  /**
   * set preview fps range value
   *
   * @param value
   */
  void set_preview_fps_range(const Range & value);

  /**
   * get a list of supported preview formats
   *
   * @return std::vector<std::string>
   */
  std::vector<std::string> get_supported_preview_formats() const;

  /**
   * get current preview format
   *
   * @return std::string
   */
  std::string get_preview_format() const;

  /**
   * set preview format
   *
   * @param value
   */
  void set_preview_format(const std::string & value);

  /**
   * set picture format (HAL3 only)
   *
   * @param value
   */
  void set_picture_format(const std::string & value);

  /**
   * set manual exposure in sensor
   *
   * @param value
   */
  void set_manual_exposure(int value);

  /**
   * set manual gain in sensor
   *
   * @param value
   */
  void set_manual_gain(int value);

  /**
   * get range of manual gain value accepted by by sensor
   *
   * @param size Image Size
   & @param fps*
   *
   * @return Range : Min and Max values
   */
  Range get_manual_gain_range(ImageSize size, int fps);
  /**
   * get manual exposure time
   *
   * @param value
   */
  int get_exposure_time() const;
  /**
   * set manual exposure time
   *
   * @param value
   */
  void set_exposure_time(const std::string & value);
  /**
   * set vertical flip bit in sensor
   *
   * @param value
   */
  void set_vertical_flip(bool value);

  /**
   * set horizontal mirror bit in sensor
   *
   * @param value
   */
  void set_horizontal_mirror(bool value);

  /**
   * set stats logging mask
   *
   * @param value
   */
  void set_stats_logging_mask(int value);

  /**
   * set anti-banding
   *
   * @param value
   */
  void set_antibanding(const std::string & value);

  /**
   * Get exposure time (ns) of the requested frame (HAL3 only)
   *
   * @param value
   * @return Frame exposure time in nSec
   */
  uint64_t get_frame_exposure_time(ICameraFrame * frame);

  /**
   * Get gain settings of the requested frame (HAL3 only)
   *
   * @param value
   */
  int32_t get_frame_gain_value(ICameraFrame * frame);

  /**
   * Get rolling shutter skew settings of the requested frame
   * (HAL3 only)
   *
   * @param value
   * @return Rolling shutter skew duration in nSec
   */
  uint64_t get_frame_rolling_shutter_skew(ICameraFrame * frame);

  /**
   * Get readout timestamp of the requested frame (HAL3 only)
   *
   *@param value
   *@return Readout timestamp in nSec
   */
  uint64_t get_frame_readout_timestamp(ICameraFrame * frame);

  /**
   * Get readout duration of the requested frame (HAL3 only)
   *
   * @param value
   * @return Readout duration in nSecs
   */
  uint64_t get_frame_readout_duration(ICameraFrame * frame);

private:
  // #if defined(_HAL3_CAMERA_)
  std::vector<Range> get_HFRFps_range() const;
  // #endif
  /**
   * private implementation and storage handle for parameters
   */
  void * priv_;

  /**
   * handle to attached camera device
   */
  ICameraDevice * device_;
}; /* class CameraParams */

}  // namespace qrb_ros::camera

#endif /* __CAMERA_PARAMETERS_H__ */