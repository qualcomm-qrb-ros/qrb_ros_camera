/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef QRB_CAMERA__QMMF_FRAME_HPP_
#define QRB_CAMERA__QMMF_FRAME_HPP_

#include <qmmf-sdk/qmmf_recorder.h>
#include <qmmf-sdk/qmmf_recorder_params.h>

#include <functional>
#include <mutex>

#include "qrb_camera/qmmf_listener.hpp"
#include "qrb_camera/qrb_camera_common.hpp"

namespace qrb_camera
{
class QMMFFrame : public ICameraFrame
{
  using RemoveFrameCb = std::function<void(QMMFFrame * frame)>;

public:
  QMMFFrame(RemoveFrameCb cb, uint32_t session_id, uint32_t camera_id)
    : cb_(cb), camera_id_(camera_id), session_id_(session_id){};
  ~QMMFFrame();

  virtual uint32_t acquire_ref();
  virtual uint32_t release_ref();

  uint32_t get_ref()
  {
    std::unique_lock lock(refs_mutex_);
    return refs_;
  }

  void dispatch_snapshot_frame(std::vector<ICameraListener *> listeners,
      qmmf::recorder::Recorder * recorder,
      qmmf::BufferDescriptor & buffer,
      qmmf::BufferMeta & meta_buffer,
      qmmf::CameraMetadata & frameMetadata,
      uint32_t camera_id,
      uint32_t image_id,
      std::string & stream_name,
      uint32_t width,
      uint32_t heigh);

  void dispatch_track_frame(std::vector<ICameraListener *> listeners,
      qmmf::recorder::Recorder * recorder,
      std::vector<qmmf::BufferDescriptor> & buffers,
      std::vector<qmmf::BufferMeta> & meta_buffers,
      qmmf::CameraMetadata & frameMetadata,
      uint32_t camera_id,
      uint32_t frame_index,
      std::string & stream_name,
      qmmf::recorder::VideoFormat format,
      uint32_t width,
      uint32_t height);

private:
  // For Video Track case
  std::vector<qmmf::BufferDescriptor> buffers_;
  std::vector<qmmf::BufferMeta> meta_buffers_;
  // For Snapshot case
  qmmf::BufferDescriptor buffer_;
  qmmf::BufferMeta meta_buffer_;
  qmmf::recorder::Recorder * recorder_;

  RemoveFrameCb cb_;

  uint32_t camera_id_;
  uint32_t session_id_;
  uint32_t buf_id_;
  StreamType type_;

  std::mutex refs_mutex_;
  std::string logger_ = "QMMFFrame";
};
}  // namespace qrb_camera

#endif