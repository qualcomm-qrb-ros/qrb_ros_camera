/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef CAMERA_BACKEND_QMMF__QMMF_FRAME_HPP_
#define CAMERA_BACKEND_QMMF__QMMF_FRAME_HPP_

#include <qmmf-sdk/qmmf_recorder.h>
#include <qmmf-sdk/qmmf_recorder_params.h>

#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "camera_interface/camera_interface.hpp"

namespace qrb_camera
{
// A QMMF buffer handed to the listeners. Owns the buffer: the destructor returns it to the
// recorder. The reference count is real: when it drops to 0, RemoveFrameCb removes and
// deletes the frame (see QMMFCamera::remove_frame), which returns the buffer.
class QMMFFrame : public ICameraFrame
{
  using RemoveFrameCb = std::function<void(QMMFFrame * frame)>;

public:
  QMMFFrame(RemoveFrameCb cb, uint32_t session_id, uint32_t camera_id)
    : cb_(std::move(cb)), camera_id_(camera_id), session_id_(session_id)
  {
  }
  ~QMMFFrame() override;

  uint32_t acquire_ref() override;
  uint32_t release_ref() override;

  void dispatch_snapshot_frame(std::vector<ICameraListener *> & listeners,
      qmmf::recorder::Recorder * recorder,
      qmmf::BufferDescriptor & buffer,
      qmmf::BufferMeta & meta_buffer,
      uint32_t camera_id,
      uint32_t image_id,
      std::string & stream_name,
      uint32_t width,
      uint32_t height);

  void dispatch_track_frame(std::vector<ICameraListener *> & listeners,
      qmmf::recorder::Recorder * recorder,
      std::vector<qmmf::BufferDescriptor> & buffers,
      std::vector<qmmf::BufferMeta> & meta_buffers,
      uint32_t camera_id,
      uint32_t frame_index,
      std::string & stream_name,
      uint32_t width,
      uint32_t height);

private:
  // Video track
  std::vector<qmmf::BufferDescriptor> buffers_;
  std::vector<qmmf::BufferMeta> meta_buffers_;
  // Snapshot
  qmmf::BufferDescriptor buffer_;
  qmmf::BufferMeta meta_buffer_;

  // nullptr until a buffer is attached: the destructor then has nothing to return.
  qmmf::recorder::Recorder * recorder_ = nullptr;
  RemoveFrameCb cb_;

  uint32_t camera_id_;
  uint32_t session_id_;
  uint32_t buf_id_ = 0;
  StreamType type_ = StreamType::Track;

  std::mutex refs_mutex_;
  std::string logger_ = "QMMFFrame";
};
}  // namespace qrb_camera

#endif  // CAMERA_BACKEND_QMMF__QMMF_FRAME_HPP_
