/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

// Ported from qrb_ros_camera (IQ9) qrb_camera/src/qmmf_camera.cpp. The vendor call order is
// unchanged. Changes, each marked "Fix:" below: no frame leak when no listener is attached
// or the buffers are empty, no iterator invalidation in stop_camera(), streams rebuilt on
// every init() (restart works), defined behaviour for an unknown format, and the recorder
// is stopped / disconnected on destruction after a failed start.

#include "qmmf_camera.hpp"

#include <time.h>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <unordered_set>

using namespace qmmf;
using namespace recorder;

namespace qrb_camera
{
QMMFCamera::QMMFCamera(uint32_t camera_id) : ICameraDevice(camera_id) {}

QMMFCamera::~QMMFCamera()
{
  if (is_working_) {
    stop_camera();
    return;
  }
  // Fix: a failed init() or start_camera() left the camera started or connected.
  if (camera_started_) {
    recorder_.StopCamera(camera_id_);
    camera_started_ = false;
  }
  if (connected_) {
    disconnect_recorder();
  }
}

void QMMFCamera::set_param(std::vector<CameraParam> & param)
{
  param_ = param;
}

void QMMFCamera::clear_streams()
{
  track_streams_.clear();
  snap_streams_.clear();
  session_ids_.clear();
  snap_ids_.clear();
}

int64_t QMMFCamera::now_ns() const
{
  struct timespec ts;
  clock_gettime(CLOCK_BOOTTIME, &ts);
  return static_cast<int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

bool QMMFCamera::init()
{
  if (is_working_) {
    std::cout << "[WARNING] [" << logger_ << "]: init while the camera is running" << std::endl;
    return false;
  }
  // Fix: init() runs before every start; rebuild the streams instead of appending to them.
  clear_streams();

  if (!connected_) {
    status_t ret = connect_recorder();
    if (ret != 0) {
      std::cout << "[ERROR] [" << logger_ << "]: qmmf_recorder_connect failed." << std::endl;
      return false;
    }
    connected_ = true;
  }

  using namespace std::placeholders;
  auto trackdata_cb = std::bind(&QMMFCamera::track_data_cb, this, _1, _2, _3, _4, _5, _6, _7);
  auto trackevent_cb = std::bind(&QMMFCamera::track_event_cb, this, _1, _2, _3, _4);
  auto snap_cb = std::bind(&QMMFCamera::snapshot_cb, this, _1, _2, _3, _4, _5, _6);
  uint32_t stream_id = 1;
  uint32_t snap_id = 1;
  for (auto & param : param_) {
    for (auto t_param : param.track_params) {
      QMMFStream stream;
      // Fix: the original left `format` uninitialized for an unknown format string.
      qmmf::recorder::VideoFormat format = VideoFormat::kNV12;
      if (t_param.format == "nv12") {
        format = VideoFormat::kNV12;
      } else if (t_param.format == "y16") {
        format = VideoFormat::kBayerRDI16BIT;
      } else if (t_param.format == "jpeg") {
        format = VideoFormat::kJPEG;
      } else if (t_param.format == "raw16") {
        format = VideoFormat::kNV12;  // as in the original code
      } else {
        std::cout << "[WARNING] [" << logger_ << "]: unsupported format '" << t_param.format
                  << "' for stream '" << t_param.name << "', using nv12" << std::endl;
      }
      if (!stream.init(camera_id_, t_param, format, stream_id, &recorder_, trackdata_cb,
              trackevent_cb, snap_cb, t_param.name, StreamType::Track)) {
        return false;
      }
      track_streams_.push_back(stream);
      session_ids_.push_back(stream_id);
      stream_id++;
    }

    for (auto s_param : param.snapshot_params) {
      QMMFStream stream;
      if (!stream.init(camera_id_, s_param, VideoFormat::kNV12, snap_id, &recorder_, trackdata_cb,
              trackevent_cb, snap_cb, s_param.name, StreamType::Snapshot)) {
        return false;
      }
      snap_streams_.push_back(stream);
      snap_ids_.push_back(snap_id);
      snap_id++;
    }
  }
  return true;
}

status_t QMMFCamera::connect_recorder()
{
  ::qmmf::recorder::RecorderCb recorder_status_cb;
  recorder_status_cb.event_cb = [this](qmmf::recorder::EventType event_type, void * event_data,
                                    size_t event_data_size) {
    (void)event_data;
    (void)event_data_size;
    if (event_type == EventType::kCameraError) {
      std::cout << "[ERROR] [" << logger_ << "]: Find CameraError in recorder connect."
                << std::endl;
    }
  };
  return recorder_.Connect(recorder_status_cb);
}

status_t QMMFCamera::disconnect_recorder()
{
  std::cout << "[INFO] [" << logger_ << "]: disconnect camera server" << std::endl;
  connected_ = false;
  return recorder_.Disconnect();
}

int QMMFCamera::start_camera()
{
  std::cout << "[INFO] [" << logger_ << "]: start camera." << std::endl;
  if (!connected_) {
    std::cout << "[ERROR] [" << logger_ << "]: start camera before init." << std::endl;
    return -1;
  }
  status_t ret = recorder_.StartCamera(camera_id_, 30);
  if (0 != ret) {
    std::cout << "[ERROR] [" << logger_ << "]: start camera failed." << std::endl;
    return -1;
  }
  camera_started_ = true;

  if (!session_ids_.empty()) {
    for (size_t i = 0; i < track_streams_.size(); i++) {
      ret = track_streams_[i].create_stream();
      if (0 != ret) {
        std::cout << "[ERROR] [" << logger_ << "]: create track stream failed! stream id: "
                  << track_streams_[i].get_stream_id() << std::endl;
        return -1;
      }
    }
    std::unordered_set<uint32_t> set(session_ids_.begin(), session_ids_.end());
    ret = recorder_.StartVideoTracks(set);
    if (0 != ret) {
      std::cout << "[ERROR] [" << logger_ << "]: start track failed!" << std::endl;
      return -1;
    }
  }

  for (size_t i = 0; i < snap_streams_.size(); i++) {
    ret = snap_streams_[i].create_stream();
    if (0 != ret) {
      std::cout << "[ERROR] [" << logger_
                << "]: create snap stream failed! stream id: " << snap_streams_[i].get_stream_id()
                << std::endl;
      return -1;
    }
  }
  is_working_ = true;
  return 0;
}

int QMMFCamera::stop_camera()
{
  if (!is_working_) {
    return 0;
  }
  std::cout << "[INFO] [" << logger_ << "]: stop camera" << std::endl;
  int result = 0;
  status_t ret;

  // Fix: the original erased from snap_streams_ while iterating it. Errors are logged and
  // the teardown goes on, so that the next steps still release their resources.
  for (auto & stream : snap_streams_) {
    if (stream.delete_stream() != 0) {
      std::cout << "[ERROR] [" << logger_
                << "]: delete snap stream failed! stream id: " << stream.get_stream_id()
                << std::endl;
      result = -1;
    }
  }

  if (!session_ids_.empty()) {
    std::unordered_set<uint32_t> set(session_ids_.begin(), session_ids_.end());
    ret = recorder_.StopVideoTracks(set);
    if (0 != ret) {
      std::cout << "[ERROR] [" << logger_ << "]: stop track failed" << std::endl;
      result = -1;
    }
  }

  // Return the buffers still in flight: after StopVideoTracks, before DeleteVideoTrack.
  {
    std::lock_guard<std::mutex> lck(frame_mutex_);
    if (!frames_.empty()) {
      std::cout << "[INFO] [" << logger_ << "]: return buffer" << std::endl;
    }
    for (auto * frame : frames_) {
      delete frame;
    }
    frames_.clear();
  }

  for (auto & stream : track_streams_) {
    if (stream.delete_stream() != 0) {
      std::cout << "[ERROR] [" << logger_
                << "]: delete track stream failed! stream id: " << stream.get_stream_id()
                << std::endl;
      result = -1;
    }
  }
  clear_streams();

  ret = recorder_.StopCamera(camera_id_);
  camera_started_ = false;
  if (0 != ret) {
    std::cout << "[ERROR] [" << logger_ << "]: stop camera failed" << std::endl;
    result = -1;
  }
  if (disconnect_recorder() != 0) {
    result = -1;
  }
  is_working_ = false;
  return result;
}

void QMMFCamera::add_listener(ICameraListener * listener)
{
  std::lock_guard<std::mutex> lck(listener_mutex_);
  auto it = std::find(listeners_.begin(), listeners_.end(), listener);
  if (it != listeners_.end()) {
    std::cout << "[WARNING] [" << logger_ << "]: this listener is already added" << std::endl;
    return;
  }
  listeners_.push_back(listener);
}

void QMMFCamera::remove_listener(ICameraListener * listener)
{
  std::lock_guard<std::mutex> lck(listener_mutex_);
  auto it = std::find(listeners_.begin(), listeners_.end(), listener);
  if (it != listeners_.end()) {
    listeners_.erase(it);
    return;
  }
  std::cout << "[WARNING] [" << logger_ << "]: this listener not add in before" << std::endl;
}

void QMMFCamera::track_data_cb(uint32_t track_id,
    std::vector<qmmf::BufferDescriptor> buffers,
    std::vector<qmmf::BufferMeta> meta_buffers,
    std::string & stream_name,
    qmmf::recorder::VideoTrackParam & param,
    uint32_t frame_index,
    int32_t orientation)
{
  // Held during the dispatch: remove_listener() waits until no frame is being delivered.
  std::lock_guard<std::mutex> lck(listener_mutex_);
  // Fix: the original allocated the frame first and leaked it on these early returns.
  if (listeners_.empty() || buffers.empty() || meta_buffers.empty()) {
    if (listeners_.empty()) {
      std::cout << "[WARNING] [" << logger_ << "]: track data listener is empty" << std::endl;
    } else {
      std::cout << "[ERROR] [" << logger_ << "]: empty buffer in track data callback" << std::endl;
    }
    recorder_.ReturnTrackBuffer(track_id, buffers);
    return;
  }

  using namespace std::placeholders;
  auto cb = std::bind(&QMMFCamera::remove_frame, this, _1);
  auto * frame = new QMMFFrame(cb, track_id, camera_id_);
  frame->orientation = orientation;
  {
    std::lock_guard<std::mutex> flck(frame_mutex_);
    frames_.push_back(frame);
  }
  frame->dispatch_track_frame(listeners_, &recorder_, buffers, meta_buffers, camera_id_,
      frame_index, stream_name, param.width, param.height);
}

void QMMFCamera::remove_frame(QMMFFrame * frame)
{
  std::lock_guard<std::mutex> lck(frame_mutex_);
  auto it = std::find(frames_.begin(), frames_.end(), frame);
  if (it != frames_.end()) {
    frames_.erase(it);
    delete frame;  // returns the buffer
    return;
  }
  std::cout << "[ERROR] [" << logger_ << "]: this frame is not add in before" << std::endl;
}

void QMMFCamera::track_event_cb(uint32_t track_id,
    qmmf::recorder::EventType event_type,
    void * event_data,
    size_t event_data_size)
{
  (void)track_id;
  (void)event_type;
  (void)event_data;
  (void)event_data_size;
  std::cout << "[INFO] [" << logger_ << "]: receive track event" << std::endl;
}

void QMMFCamera::snapshot_cb(uint32_t camera_id,
    uint32_t image_count,
    qmmf::BufferDescriptor buffer,
    qmmf::BufferMeta meta,
    std::string & stream_name,
    qmmf::recorder::VideoTrackParam & param)
{
  std::lock_guard<std::mutex> lck(listener_mutex_);
  // Fix: same leak as in track_data_cb.
  if (listeners_.empty() || buffer.size == 0) {
    std::cout << "[WARNING] [" << logger_ << "]: snapshot data listener is empty or the "
              << "buffer is empty" << std::endl;
    recorder_.ReturnImageCaptureBuffer(camera_id, buffer);
    return;
  }

  using namespace std::placeholders;
  auto cb = std::bind(&QMMFCamera::remove_frame, this, _1);
  auto * frame = new QMMFFrame(cb, 0, camera_id_);
  {
    std::lock_guard<std::mutex> flck(frame_mutex_);
    frames_.push_back(frame);
  }
  frame->dispatch_snapshot_frame(listeners_, &recorder_, buffer, meta, camera_id_, image_count,
      stream_name, param.width, param.height);
}
}  // namespace qrb_camera
