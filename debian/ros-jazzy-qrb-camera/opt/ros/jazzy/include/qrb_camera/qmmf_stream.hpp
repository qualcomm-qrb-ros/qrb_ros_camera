/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef QRB_CAMERA__QMMF_STREAM_HPP_
#define QRB_CAMERA__QMMF_STREAM_HPP_

#include <qmmf-sdk/qmmf_recorder.h>
#include <qmmf-sdk/qmmf_recorder_params.h>

#include <functional>

#include "qrb_camera/qmmf_listener.hpp"
#include "qrb_camera/qrb_camera_common.hpp"

namespace qrb_camera
{
class QMMFStream
{
  using SnapshotCb = std::function<void(uint32_t camera_id,
      uint32_t image_count,
      qmmf::BufferDescriptor buffer,
      qmmf::BufferMeta meta,
      std::string & stream_name,
      qmmf::recorder::VideoTrackParam & param)>;

  using TrackDataCb = std::function<void(uint32_t track_id,
      std::vector<qmmf::BufferDescriptor> buffers,
      std::vector<qmmf::BufferMeta> meta_buffers,
      std::string & stream_name,
      qmmf::recorder::VideoTrackParam & param,
      uint32_t frame_index,
      int32_t orientation)>;

  using TrackEventCb = std::function<void(uint32_t track_id,
      qmmf::recorder::EventType event_type,
      void * event_data,
      size_t event_data_size)>;

public:
  bool init(uint32_t camera_id,
      StreamParam param,
      qmmf::recorder::VideoFormat format,
      uint32_t stream_id,
      qmmf::recorder::Recorder * recorder,
      TrackDataCb data_cb,
      TrackEventCb event_cb,
      SnapshotCb snap_cb,
      std::string & stream_name,
      const StreamType & type);
  status_t create_stream();
  status_t delete_stream();

  uint32_t get_stream_id() { return stream_id_; }

private:
  uint32_t stream_id_;
  std::string stream_name_;
  qmmf::recorder::VideoTrackParam stream_param_;
  qmmf::recorder::ImageParam image_param_;
  qmmf::recorder::Recorder * recorder_;
  TrackDataCb data_cb_;
  TrackEventCb event_cb_;
  SnapshotCb snap_cb_;
  StreamType type_;
  uint32_t frame_size_;
  int32_t orientation_;
  std::string logger_ = "QMMFStream";
};

}  // namespace qrb_camera

#endif