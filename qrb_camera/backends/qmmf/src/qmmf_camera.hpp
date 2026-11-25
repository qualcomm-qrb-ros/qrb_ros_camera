/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef CAMERA_BACKEND_QMMF__QMMF_CAMERA_HPP_
#define CAMERA_BACKEND_QMMF__QMMF_CAMERA_HPP_

#include <qmmf-sdk/qmmf_recorder.h>
#include <qmmf-sdk/qmmf_recorder_params.h>

#include <mutex>
#include <string>
#include <vector>

#include "camera_interface/camera_interface.hpp"
#include "qmmf_frame.hpp"
#include "qmmf_stream.hpp"

namespace qrb_camera
{
// One camera on QMMF/CamX. Owns its own Recorder connection:
//   init()          Connect, build the streams (no hardware touched)
//   start_camera()  StartCamera, create the tracks, StartVideoTracks, snapshots
//   stop_camera()   reverse order, returns the buffers in flight, Disconnect
// Frame timestamps are CLOCK_BOOTTIME (see now_ns()).
class QMMFCamera : public ICameraDevice
{
public:
  explicit QMMFCamera(uint32_t camera_id);
  ~QMMFCamera() override;

  void set_param(std::vector<CameraParam> & param) override;
  bool init() override;
  int start_camera() override;
  int stop_camera() override;
  void add_listener(ICameraListener * listener) override;
  void remove_listener(ICameraListener * listener) override;
  int64_t now_ns() const override;

private:
  status_t connect_recorder();
  status_t disconnect_recorder();
  void clear_streams();

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

  // Filled by init() and not resized afterwards: the recorder callbacks point into them.
  std::vector<QMMFStream> track_streams_;
  std::vector<QMMFStream> snap_streams_;
  std::vector<uint32_t> session_ids_;
  std::vector<uint32_t> snap_ids_;

  std::mutex listener_mutex_;
  std::vector<ICameraListener *> listeners_;
  std::vector<CameraParam> param_;

  bool connected_ = false;       // Connect succeeded, Disconnect not yet called
  bool camera_started_ = false;  // StartCamera succeeded, StopCamera not yet called

  std::string logger_ = "QMMFCamera";
};
}  // namespace qrb_camera

#endif  // CAMERA_BACKEND_QMMF__QMMF_CAMERA_HPP_
