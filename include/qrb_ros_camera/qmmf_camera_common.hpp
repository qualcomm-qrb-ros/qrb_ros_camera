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
#ifndef __QMMF_CAMERA_COMMON_H__
#define __QMMF_CAMERA_COMMON_H__
#define TAG "QMMF_CAMERA"
#include "camera_ros2_common.hpp"
#include "rclcpp/rclcpp.hpp"
namespace qrb_ros::camera
{
extern int DEFAULT_LOG_LEVEL;
#define DEBUG_LEVEL 1
#define INFO_LEVEL 2
#define WARNING_LEVEL 3
#define ERROR_LEVEL 4
#define CAM_ERR(fmt, args...)                                                                      \
  do {                                                                                             \
    if (DEFAULT_LOG_LEVEL < ERROR_LEVEL) {                                                         \
      ALOGE("%s:%d %s() " fmt, __FILENAME__, __LINE__, __FUNCTION__, ##args);                      \
      camera_ros2_log("[ERROR] %s:" STRINGIZE_MACRO(__LINE__) " %s() " fmt, __FILENAME__,          \
          __FUNCTION__, ##args);                                                                   \
    }                                                                                              \
  } while (0)
#define CAM_WARN(fmt, args...)                                                                     \
  do {                                                                                             \
    if (DEFAULT_LOG_LEVEL < WARNING_LEVEL) {                                                       \
      ALOGW("%s:%d %s() " fmt, __FILENAME__, __LINE__, __FUNCTION__, ##args);                      \
      camera_ros2_log("[WARN] %s:" STRINGIZE_MACRO(__LINE__) " %s() " fmt, __FILENAME__,           \
          __FUNCTION__, ##args);                                                                   \
    }                                                                                              \
  } while (0)
#define CAM_INFO(fmt, args...)                                                                     \
  do {                                                                                             \
    if (DEFAULT_LOG_LEVEL < INFO_LEVEL) {                                                          \
      ALOGI("%s:%d %s() " fmt, __FILENAME__, __LINE__, __FUNCTION__, ##args);                      \
      camera_ros2_log("[INFO] %s:" STRINGIZE_MACRO(__LINE__) " %s() " fmt, __FILENAME__,           \
          __FUNCTION__, ##args);                                                                   \
    }                                                                                              \
  } while (0)
#if 0
#define CAM_DBG(fmt, args...)                                                                      \
  do {                                                                                             \
    if (DEFAULT_LOG_LEVEL < DEBUG_LEVEL) {                                                         \
      ALOGD("%s:%d %s() " fmt, __FILENAME__, __LINE__, __FUNCTION__, ##args);                      \
      camera_ros2_log("[DEBUG] %s:" STRINGIZE_MACRO(__LINE__) " %s() " fmt, __FILENAME__,          \
          __FUNCTION__, ##args);                                                                   \
    }                                                                                              \
  } while (0)
#else
#define CAM_DBG(fmt, args...)                                                                      \
  do {                                                                                             \
  } while (0)
#endif
#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName)                                                         \
  TypeName(const TypeName &);                                                                      \
  void operator=(const TypeName &)
#endif

/**
 * Type of control event received from camera
 */
enum CameraEventType
{
  CameraEvtNone = 0x0000,
  CameraEvtFocus = 0x0001,
  CameraEvtAll = 0xFFFF,
};
/**
 * Interface to an object that represents single image/metadata
 * frame
 */
class ICameraFrame
{
  DISALLOW_COPY_AND_ASSIGN(ICameraFrame);

protected:
  uint32_t refs_;
  ICameraFrame() : refs_(0), timestamp(0), data(nullptr), fd(0) {}
  virtual ~ICameraFrame() {}

public:
  /**
   * aquire a reference to the frame, this is required if client
   * wants to hold the frame for further processing after camera
   * callback returns
   *
   * @return uint32_t : number of refs to the frame.
   */
  virtual uint32_t acquire_ref() = 0;
  /**
   * release reference to the frame object. This will release
   * memory associated with image data as well.
   *
   * @return uint32_t : number of refs to the frame
   */
  virtual uint32_t release_ref() = 0;
  /**
   * frame timestamp
   */
  uint64_t timestamp;
  /**
   * frame data size in bytes
   */
  uint32_t size;
  /**
   * pointer to start of image data
   */
  uint8_t * data;
  /**
   * file descriptor for the frame memory
   * If value is -1, it indicates the memory is not shared
   */
  int fd;
  /**
   * opaque metadata about the frame memory. This is used for
   * sharing memory between camera and video encoder
   */
  void * metadata;
  uint32_t stride;
  uint32_t slice;
};
/**
 * Interface to a parameters object.
 */
class CameraParams;
/**
 * Interface for camera listener object. Client needs to
 * implement this interface to get access to camera data and
 * control events. The methods in listener can be invoked from
 * multiple different threads. Client needs to make sure that
 * implementation is thread-safe.
 */
class ICameraListener
{
public:
  virtual ~ICameraListener() {}
  /**
   * This function is called when an error is generated by camera
   * driver
   */
  virtual void on_error() {}
  /**
   * This function is called when a preview frame is generated by
   * camera.
   *
   * @param frame [in]: pointer to an existing ICameraFrame
   *              generated by camera
   */
  virtual void on_preview_frame(ICameraFrame * frame) {}
  /**
   * This function is called when a preview frame is generated by
   * camera.
   *
   * @param frame [in]: pointer to an existing ICameraFrame
   *              generated by camera
   */
  virtual void on_video_frame(ICameraFrame * frame) {}
  /**
   * This function is called when a snapshot frame is generated by
   * camera.
   *
   * @param frame [in]: pointer to an existing ICameraFrame
   *              generated by camera
   */
  virtual void on_picture_frame(ICameraFrame * frame) {}
  /**
   * This function is called when a metadata frame is generated by
   * camera.
   *
   * @param frame [in]: pointer to an existing ICameraFrame
   *              generated by camera
   */
  virtual void on_metadata_frame(ICameraFrame * frame) {}
};
/**
 * Interface to a camera device. Client uses the API provided by
 * this interface to interact with camera device.
 */
class ICameraDevice
{
  DISALLOW_COPY_AND_ASSIGN(ICameraDevice);

protected:
  ICameraDevice() {}
  virtual ~ICameraDevice() {}

public:
  /**
   * Factory method to create an instance of ICameraDevice
   *
   * @param index [in]: camera ID
   * @param device [out]: pointer to to a ICameraDevice* to be
   *               created
   *
   * @return int : 0 on Success
   */
  static int create_instance(int index, ICameraDevice ** device);
  /**
   * delete instance of an ICameraDevice
   *
   * @param device [out]: pointer to an ICameraDevice* to be
   *               deleted
   */
  static void delete_instance(ICameraDevice ** device);
  /**
   * Add listener to handle various notifications and data frames from the
   * camera device.
   * @param listener [in]
   **/
  virtual void add_listener(ICameraListener * listener) = 0;
  /**
   * Removes a previously added listener from camera device
   *
   * @param listener [in]
   */
  virtual void remove_listener(ICameraListener * listener) = 0;
  /**
   * Set parameters in camera device. Camera device state will be
   * updated with new settings when this call returns.
   *
   * @param params [in]: populated parameters object with new
   *               camera parameters to be set
   *
   * @return int : 0 on success
   */
  virtual int set_parameters(const CameraParams & params) = 0;
  /**
   * Retrieve the octet stream of parameters as name,value pairs. Note the
   * parameters fetched in to be buffer may be partial to the extent of buf_size.
   * Use the bufSizeRequired to determine the total length of buffer needed to
   * get all the parameters.
   *
   * @param buf [out]: buffer will be populated with the octet stream of
   *            parameters.
   * @param buf_size [in]: sizeof memory at buf.
   * @param bufSizeRequired [out]: optional if provided will be populated
   * with total size of buffer needed to fetch all the parameters.
   *
   * @return int : 0 on success
   **/
  virtual int get_parameters(uint8_t * buf, uint32_t buf_size, int * bufSizeRequired = nullptr) = 0;
  /**
   * Gets the pointer to the the class object that define the
   * parameters of the camera sensor. (HAL3 only)
   *
   * @return pointer to parameters object typecasted to void
   **/
  virtual void * get_parameters() = 0;
  /**
   * start preview stream on camera
   *
   * @return int : 0 on success
   */
  virtual int start_preview() = 0;
  /**
   * stop preview streaming
   */
  virtual void stop_preview() = 0;
};
}  // namespace qrb_ros::camera
#endif /* !__QMMFCAMERA_H__ */
