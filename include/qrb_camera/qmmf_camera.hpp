/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef QRB_CAMERA__QMMF_CAMERA_HPP_
#define QRB_CAMERA__QMMF_CAMERA_HPP_

#include <qmmf-sdk/qmmf_recorder.h>
#include <qmmf-sdk/qmmf_recorder_params.h>

#include <mutex>
#include <vector>

#include "qrb_camera/qmmf_frame.hpp"
#include "qrb_camera/qmmf_listener.hpp"
#include "qrb_camera/qmmf_stream.hpp"
#include "qrb_camera/qrb_camera_common.hpp"

namespace qrb_camera
{
class QMMFCamera : public ICameraDevice
{
public:
  explicit QMMFCamera(uint32_t camera_id);

  void set_param(std::vector<CameraParam> & param) override;

  bool init() override;

  int start_camera() override;
  int stop_camera() override;

  virtual void add_listener(ICameraListener * listener);
  virtual void remove_listener(ICameraListener * listener);

private:
  status_t connect_recorder();
  status_t disconnect_recorder();

  void track_data_cb(uint32_t track_id,
      std::vector<qmmf::BufferDescriptor> buffers,
      std::vector<qmmf::BufferMeta> meta_buffers,
      std::string & stream_name,
      qmmf::recorder::VideoTrackParam & param,
      uint32_t frame_index,
      int32_t orientation);

  void track_event_cb(uint32_t track_id,
      qmmf::recorder::EventType event_type,
      void * event_data,
      size_t event_data_size);

  void snapshot_cb(uint32_t camera_id,
      uint32_t image_count,
      qmmf::BufferDescriptor buffer,
      qmmf::BufferMeta meta,
      std::string & stream_name,
      qmmf::recorder::VideoTrackParam & param);

  void remove_frame(QMMFFrame * frame);

  std::mutex frame_mutex_;
  qmmf::recorder::Recorder recorder_;
  std::vector<QMMFFrame *> frames_;
  // std::vector<std::shared_ptr<Frame>> frames_;
  std::vector<QMMFStream> track_streams_;
  std::vector<QMMFStream> snap_streams_;
  std::vector<uint32_t> session_ids_;
  std::vector<uint32_t> snap_ids_;
  std::mutex listener_mutex_;
  std::vector<ICameraListener *> listeners_;
  std::vector<CameraParam> param_;
  qmmf::CameraMetadata static_default_meta_info;

  std::string logger_ = "QMMFCamera";
};
}  // namespace qrb_camera
#endif