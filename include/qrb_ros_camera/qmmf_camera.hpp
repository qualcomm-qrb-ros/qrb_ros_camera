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
 *​​​​ ​Changes from Qualcomm Innovation Center, Inc. are provided
 *under the following license: Copyright (c) 2023-2024 Qualcomm Innovation
 *Center, Inc. All rights reserved. SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef __QMMF_CAMERA_H__
#define __QMMF_CAMERA_H__
#define TAG "QMMF_CAMERA"
#include <qmmf-sdk/qmmf_recorder.h>
#include <qmmf-sdk/qmmf_recorder_extra_param.h>
#include <qmmf-sdk/qmmf_recorder_params.h>
#include <string.h>
#include <system/camera_metadata.h>

#include "qmmf-sdk/qmmf_camera_metadata.h"
#include "qrb_ros_camera/qmmf_camera_common.hpp"
#include "rclcpp/rclcpp.hpp"
namespace qrb_ros::camera {
// namespace camera = android::hardware::camera::common::V1_0::helper;
// using CameraMetadata =
// android::hardware::camera::common::V1_0::helper::CameraMetadata;
using CameraMetadata = ::qmmf::CameraMetadata;
// Sleep for specified seconds to allow settling after parameter change
using namespace qmmf;
using namespace recorder;
using namespace android;
#define DEFAULT_SENSOR_FRAME_RATE 30
#define DEFAULT_SENSOR_WIDTH 640
#define DEFAULT_SENSOR_HEIGHT 480
#define RELEASE_REF_BUF_WAIT_US 33000
#define RELEASE_REF_BUF_WAIT_MAX_ATTEMPT 30
#define NO_ERROR 0

enum FrameType {
  CAMERA_QMMF_TRACK_PREVIEW = 1,
  CAMERA_QMMF_TRACK_VIDEO = 2,
  CAMERA_QMMF_TRACK_PICTURE = 3,
  CAMERA_QMMF_TRACK_MAX = 4,
};
class QMMFCameraParams {
public:
  ::qmmf::recorder::CameraExtraParam start_params;
  CameraMetadata static_default_meta_info;
  CameraMetadata static_session_meta_info;
  bool static_session_meta_info_updated_flag;
  VideoTrackParam *track_params_ptr[CAMERA_QMMF_TRACK_MAX];
  std::mutex params_mutex;
  QMMFCameraParams() {
    for (int i = 0; i < CAMERA_QMMF_TRACK_MAX; i++) {
      track_params_ptr[i] = nullptr;
    }
    static_session_meta_info_updated_flag = false;
  }
  void mutex_lock() { params_mutex.lock(); }
  void mutex_unlock() { params_mutex.unlock(); }
};
class qmmfCameraFrame : public ICameraFrame {
private:
  qmmf::recorder::Recorder *recorder_;
  std::vector<BufferDescriptor> buffers_;
  BufferDescriptor buffer_;
  std::vector<qmmf::BufferMeta> meta_buffers_;
  qmmf::BufferMeta meta_buffer_;
  CameraMetadata frame_info_metadata_;
  uint32_t camera_id_;
  uint32_t track_id_;
  FrameType type_;
  uint32_t buf_id_;
  static std::map<int, qmmfCameraFrame *> refs_fd_frame_map_;
  static std::mutex refs_map_mutex_;
  std::mutex refs_mutex_;

public:
  qmmfCameraFrame(int fd, uint32_t size);
  virtual ~qmmfCameraFrame();
  virtual uint32_t acquire_ref();
  virtual uint32_t release_ref();
  static void dispatch_frame(std::vector<ICameraListener *> listeners,
                             qmmf::recorder::Recorder *recorder,
                             BufferDescriptor &buffers,
                             qmmf::BufferMeta &meta_buffers,
                             CameraMetadata &frameMetadata, uint32_t camera_id,
                             uint32_t track_id);
  static void dispatch_frame(std::vector<ICameraListener *> listeners,
                             qmmf::recorder::Recorder *recorder,
                             std::vector<qmmf::BufferDescriptor> &buffers,
                             std::vector<qmmf::BufferMeta> &meta_buffers,
                             CameraMetadata &frameMetadata, uint32_t camera_id,
                             uint32_t track_id);
  virtual CameraMetadata *get_frame_info_metadata_ptr() {
    return static_cast<CameraMetadata *>(&frame_info_metadata_);
  }
  friend status_t release_all_frames();
};
class qmmfCAMERA;
class QMMFTrack {
private:
  uint32_t track_id_;
  std::unordered_set<uint32_t> track_ids;
  VideoTrackParam track_params_;
  Recorder *recorder_ptr_;
  qmmfCAMERA *qmmf_camera_ptr_;

public:
  status_t create_qmmf_track();
  status_t delete_qmmf_track();
  QMMFTrack();
  int init(qmmfCAMERA *qmmf_camera_ptr, Recorder *recorder_ptr,
           uint32_t camera_id, uint32_t track_id,
           VideoTrackParam **track_params_ptr);
};
class qmmfCAMERA : public ICameraDevice {
  std::vector<ICameraListener *> listeners_;
  bool is_preview_running_;
  status_t qmmf_recorder_connect();
  status_t qmmf_recorder_disconnect();
  void connect_event_cb(EventType event_type, void *event_data,
                        size_t event_data_size);
  status_t qmmf_start_camera();
  status_t qmmf_stop_camera();
  void camera_metadata_cb(uint32_t camera_id, const CameraMetadata &result);
  void session_callback_handler(EventType event_type, void *event_data,
                                size_t event_data_size);
  status_t qmmf_start_track(FrameType trackType);
  // qmmf client
  Recorder recorder_;
  QMMFCameraParams qmmf_camera_params_;
  uint32_t camera_id_;
  QMMFTrack qmmfTrackArray_[CAMERA_QMMF_TRACK_MAX];
  std::map<uint64_t, CameraMetadata> result_metadata_cache_;
  std::mutex result_metadata_cache_mutex_;

public:
  qmmfCAMERA();
  virtual ~qmmfCAMERA();
  int init(uint32_t index);
  int get_id() { return camera_id_; }
  /* Implementation of virtual methods of ICameraDevice interface */
  virtual void add_listener(ICameraListener *listener);
  virtual void remove_listener(ICameraListener *listener);
  virtual int set_parameters(const CameraParams &params);
  virtual int get_parameters(uint8_t *buf, uint32_t buf_size,
                             int *bufSizeRequired);
  virtual void *get_parameters();
  virtual int start_preview();
  virtual void stop_preview();
  void qmmf_track_event_cb(uint32_t track_id, EventType event_type,
                           void *event_data, size_t event_data_size);
  void qmmf_track_data_cb(uint32_t track_id,
                          std::vector<BufferDescriptor> buffers,
                          std::vector<qmmf::BufferMeta> meta_buffers);
};
} // namespace qrb_ros::camera
#endif /* __QMMFCAMERA_H__ */
