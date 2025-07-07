/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include "qrb_camera/qmmf_camera.hpp"

#include <iostream>

using namespace qmmf;
using namespace recorder;

namespace qrb_camera
{
void QMMFCamera::set_param(std::vector<CameraParam> & param)
{
  param_ = param;
}

QMMFCamera::QMMFCamera(uint32_t camera_id) : ICameraDevice(camera_id)
{
  type_ = CameraType::QMMF;
}

bool QMMFCamera::init()
{
  status_t ret = EXIT_SUCCESS;
  ret = connect_recorder();
  if (ret != 0) {
    std::cout << "[ERROR] [" << logger_ << "]: qmmf_recorder_connect failed." << std::endl;
    return false;
  }
  using namespace std::placeholders;
  auto trackdata_cb = std::bind(&QMMFCamera::track_data_cb, this, _1, _2, _3, _4, _5, _6, _7);
  auto trackevent_cb = std::bind(&QMMFCamera::track_event_cb, this, _1, _2, _3, _4);
  auto snap_cb = std::bind(&QMMFCamera::snapshot_cb, this, _1, _2, _3, _4, _5, _6);
  uint32_t stream_id = 1;
  uint32_t snap_id = 1;
  for (auto i = 0; i < param_.size(); i++) {
    auto param = param_[i];
    for (auto j = 0; j < param.track_params.size(); j++) {
      auto t_param = param.track_params[j];
      QMMFStream stream;
      qmmf::recorder::VideoFormat format;
      if (t_param.format == "nv12") {
        format = VideoFormat::kNV12;
      } else if (t_param.format == "rgb") {
        // format = VideoFormat::kRGB;
      } else if (t_param.format == "y16") {
        format = VideoFormat::kBayerRDI16BIT;
      } else if (t_param.format == "jpeg") {
        format = VideoFormat::kJPEG;
      } else if (t_param.format == "raw16") {
        format = VideoFormat::kNV12;
      }
      stream.init(camera_id_, t_param, format, stream_id, &recorder_, trackdata_cb, trackevent_cb,
          snap_cb, t_param.name, StreamType::Track);
      track_streams_.push_back(stream);
      session_ids_.push_back(stream_id);
      stream_id++;
    }

    for (auto j = 0; j < param.snapshot_params.size(); j++) {
      auto s_param = param.snapshot_params[j];
      QMMFStream stream;
      qmmf::recorder::VideoFormat format;
      stream.init(camera_id_, s_param, format, snap_id, &recorder_, trackdata_cb, trackevent_cb,
          snap_cb, s_param.name, StreamType::Snapshot);
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
  recorder_status_cb.event_cb = [&](qmmf::recorder::EventType event_type, void * event_data,
                                    size_t event_data_size) {
    if (event_type == EventType::kCameraError) {
      std::cout << "[ERROR] [" << logger_ << "]: Find CameraError in recorder connect."
                << std::endl;
    }
  };
  auto ret = recorder_.Connect(recorder_status_cb);
  return ret;
}

int QMMFCamera::start_camera()
{
  std::cout << "[INFO] [" << logger_ << "]: start camera." << std::endl;
  status_t ret = recorder_.StartCamera(camera_id_, 30);
  if (0 != ret) {
    std::cout << "[ERROR] [" << logger_ << "]: start camera failed." << std::endl;
    return -1;
  }
  if (session_ids_.size() != 0) {
    for (auto i = 0; i < track_streams_.size(); i++) {
      ret = track_streams_[i].create_stream();
      if (0 != ret) {
        std::cout << "[ERROR] [" << logger_
                  << "]: create track stream failed! stream id: " << session_ids_[i] << std::endl;
        session_ids_.erase(session_ids_.begin() + i);
        return -1;
      }
    }
    std::unordered_set<unsigned int> set(session_ids_.begin(), session_ids_.end());
    ret = recorder_.StartVideoTracks(set);
    if (0 != ret) {
      std::cout << "[ERROR] [" << logger_ << "]: start track failed!" << std::endl;
      return -1;
    }
  }

  if (snap_ids_.size() != 0) {
    for (auto i = 0; i < snap_streams_.size(); i++) {
      ret = snap_streams_[i].create_stream();
      if (0 != ret) {
        std::cout << "[ERROR] [" << logger_
                  << "]: create snap stream failed! stream id: " << session_ids_[i] << std::endl;
        snap_ids_.erase(snap_ids_.begin() + i);
        return -1;
      }
    }
  }
  is_working_ = true;
  return 0;
}

int QMMFCamera::stop_camera()
{
  std::cout << "[INFO] [" << logger_ << "]: stop camera" << std::endl;
  status_t ret;
  if (snap_ids_.size() != 0) {
    for (auto it = snap_streams_.begin(); it != snap_streams_.end(); it++) {
      ret = (*it).delete_stream();
      if (0 != ret) {
        std::cout << "[ERROR] [" << logger_
                  << "]: delete snap stream failed! stream id: " << (*it).get_stream_id()
                  << std::endl;
        return -1;
      }
      snap_streams_.erase(it);
    }
    snap_ids_.clear();
  }
  std::unordered_set<unsigned int> set(session_ids_.begin(), session_ids_.end());
  ret = recorder_.StopVideoTracks(set);
  if (0 != ret) {
    std::cout << "[ERROR] [" << logger_ << "]: stop track failed" << std::endl;
    return -1;
  }

  for (auto it = track_streams_.begin(); it != track_streams_.end(); it++) {
    ret = (*it).delete_stream();
    if (0 != ret) {
      std::cout << "[ERROR] [" << logger_
                << "]: delete track stream failed! stream id: " << (*it).get_stream_id()
                << std::endl;
      return -1;
      track_streams_.erase(it);
    }
  }
  session_ids_.clear();
  ret = recorder_.StopCamera(camera_id_);
  if (0 != ret) {
    std::cout << "[ERROR] [" << logger_ << "]: stop camera failed" << std::endl;
    return -1;
  }
  ret = disconnect_recorder();
  is_working_ = false;
  return ret;
}

status_t QMMFCamera::disconnect_recorder()
{
  std::cout << "[INFO] [" << logger_ << "]: disconnect camera server" << std::endl;
  auto ret = recorder_.Disconnect();
  return ret;
}

void QMMFCamera::add_listener(ICameraListener * listener)
{
  std::unique_lock<std::mutex> lck(listener_mutex_);
  auto it = std::find(listeners_.begin(), listeners_.end(), listener);
  if (it != listeners_.end()) {
    std::cout << "[WARNING] [" << logger_ << "]: this listener is already added" << std::endl;
    return;
  }
  listeners_.push_back(listener);
}

void QMMFCamera::remove_listener(ICameraListener * listener)
{
  std::unique_lock<std::mutex> lck(listener_mutex_);
  auto it = std::find(listeners_.begin(), listeners_.end(), listener);
  if (it != listeners_.end()) {
    listeners_.erase(it);
    std::cout << "[INFO] [" << logger_ << "]: remove this listener successfully" << std::endl;
    return;
  }
  std::cout << "[WARNING] [" << logger_ << "]: this listener not add in before" << std::endl;
  return;
}

void QMMFCamera::track_data_cb(uint32_t track_id,
    std::vector<qmmf::BufferDescriptor> buffers,
    std::vector<qmmf::BufferMeta> meta_buffers,
    std::string & stream_name,
    qmmf::recorder::VideoTrackParam & param,
    uint32_t frame_index,
    int32_t orientation)
{
  // std::cout << "track data callback call" << std::endl;
  using namespace std::placeholders;
  auto cb = std::bind(&QMMFCamera::remove_frame, this, _1);
  auto frame = new QMMFFrame(cb, track_id, camera_id_);
  {
    std::unique_lock<std::mutex> lck(frame_mutex_);
    frames_.push_back(frame);
  }
  // std::cout << "push frame address: " << frame << std::endl;
  CameraMetadata result;
  frame->orientation = orientation;
  std::unique_lock<std::mutex> lck(listener_mutex_);
  if (listeners_.empty()) {
    std::cout << "[WARNING] [" << logger_ << "]: track data listener is empty" << std::endl;
    return;
  }
  frame->dispatch_track_frame(listeners_, &recorder_, buffers, meta_buffers, result, camera_id_,
      frame_index, stream_name, param.format, param.width, param.height);
}

void QMMFCamera::remove_frame(QMMFFrame * frame)
{
  // std::cout << "remove frame address: " << frame << std::endl;
  std::unique_lock<std::mutex> lck(frame_mutex_);
  auto it = std::find(frames_.begin(), frames_.end(), frame);
  if (it != frames_.end()) {
    frames_.erase(it);
    delete frame;
    // std::cout << "qmmf camera: remove this frame successfully" << std::endl;
    return;
  }
  std::cout << "[ERROR] [" << logger_ << "]: this frame is not add in before" << std::endl;
  return;
}

void QMMFCamera::track_event_cb(uint32_t track_id,
    qmmf::recorder::EventType event_type,
    void * event_data,
    size_t event_data_size)
{
  std::cout << "[INFO] [" << logger_ << "]: receive track event" << std::endl;
}

void QMMFCamera::snapshot_cb(uint32_t camera_id,
    uint32_t image_count,
    qmmf::BufferDescriptor buffer,
    qmmf::BufferMeta meta,
    std::string & stream_name,
    qmmf::recorder::VideoTrackParam & param)
{
  using namespace std::placeholders;
  auto cb = std::bind(&QMMFCamera::remove_frame, this, _1);
  auto frame = new QMMFFrame(cb, 0, camera_id_);
  {
    std::unique_lock<std::mutex> lck(frame_mutex_);
    frames_.push_back(frame);
  }
  CameraMetadata result;
  std::unique_lock<std::mutex> lck(listener_mutex_);
  if (listeners_.empty()) {
    return;
  }
  frame->dispatch_snapshot_frame(listeners_, &recorder_, buffer, meta, result, camera_id_,
      image_count, stream_name, param.width, param.height);
}

}  // namespace qrb_camera