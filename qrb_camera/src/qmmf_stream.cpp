/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include "qrb_camera/qmmf_stream.hpp"

#include <iostream>

using namespace qmmf;
using namespace recorder;

namespace qrb_camera
{
bool QMMFStream::init(uint32_t camera_id,
    StreamParam param,
    qmmf::recorder::VideoFormat format,
    uint32_t stream_id,
    qmmf::recorder::Recorder * recorder,
    TrackDataCb data_cb,
    TrackEventCb event_cb,
    SnapshotCb snap_cb,
    std::string & stream_name,
    const StreamType & type)
{
  if (recorder == nullptr) {
    std::cout << "[ERROR] [" << logger_ << "]: recorder is null in Track init." << std::endl;
    return false;
  }
  stream_id_ = stream_id;
  type_ = type;
  stream_param_.camera_id = camera_id;
  if (type_ == StreamType::Track) {
    stream_param_.width = param.width;
    stream_param_.height = param.height;
    stream_param_.framerate = param.frame_rate;
    stream_param_.format = format;
  } else {
    image_param_.width = param.width;
    image_param_.height = param.height;
    image_param_.quality = 60;
  }
  recorder_ = recorder;
  data_cb_ = data_cb;
  event_cb_ = event_cb;
  snap_cb_ = snap_cb;
  stream_name_ = stream_name;
  frame_size_ = 0;
  orientation_ = param.orientation;
  return true;
}

status_t QMMFStream::create_stream()
{
  status_t ret = EXIT_SUCCESS;
  if (type_ == StreamType::Track) {
    qmmf::recorder::TrackCb video_track_cb;
    qmmf::recorder::VideoExtraParam extraparam;
    video_track_cb.data_cb = [&](uint32_t track_id, std::vector<qmmf::BufferDescriptor> buffers,
                                 std::vector<qmmf::BufferMeta> meta_buffers) {
      frame_size_++;
      data_cb_(
          track_id, buffers, meta_buffers, stream_name_, stream_param_, frame_size_, orientation_);
    };
    video_track_cb.event_cb = [&](uint32_t track_id, qmmf::recorder::EventType event_type,
                                  void * event_data, size_t data_size) {
      event_cb_(track_id, event_type, event_data, data_size);
    };
    ret = recorder_->CreateVideoTrack(stream_id_, stream_param_, extraparam, video_track_cb);
    if (0 != ret) {
      std::cout << "[ERROR] [" << logger_ << "]: CreateVideoTrack failed." << std::endl;
    }
  } else if (type_ == StreamType::Snapshot) {
    uint32_t camera_id = stream_param_.camera_id;

    ImageExtraParam xtraparam;
    ret = recorder_->ConfigImageCapture(camera_id, stream_id_, image_param_, xtraparam);

    std::vector<qmmf::CameraMetadata> meta_array;
    qmmf::CameraMetadata meta;
    auto snap_type = qmmf::recorder::SnapshotType::kVideo;
    ret = recorder_->GetDefaultCaptureParam(stream_param_.camera_id, meta);
    if (0 != ret) {
      std::cout << "[ERROR] [" << logger_ << "]: GetDefaultCaptureParam failed." << std::endl;
      return ret;
    }
    meta_array.push_back(meta);
    auto cb = [&](uint32_t camera_id, uint32_t image_count, qmmf::BufferDescriptor buffer,
                  qmmf::BufferMeta meta) -> void {
      snap_cb_(camera_id, image_count, buffer, meta, stream_name_, stream_param_);
    };
    ret = recorder_->CaptureImage(stream_param_.camera_id, snap_type, 0, meta_array, cb);
    if (0 != ret) {
      std::cout << "[ERROR] [" << logger_ << "]: CaptureImage failed." << std::endl;
    }
  }
  return ret;
}

status_t QMMFStream::delete_stream()
{
  status_t ret = EXIT_SUCCESS;
  if (type_ == StreamType::Track) {
    ret = recorder_->DeleteVideoTrack(stream_id_);
    if (ret != 0) {
      std::cout << "[ERROR] [" << logger_ << "]: DeleteVideoTrack failed." << std::endl;
      return ret;
    }
    std::cout << "[INFO] [" << logger_ << "]: DeleteVideoTrack success." << std::endl;
  } else if (type_ == StreamType::Snapshot) {
    uint32_t camera_id = stream_param_.camera_id;
    ret = recorder_->CancelCaptureImage(camera_id, stream_id_);
    if (ret != 0) {
      std::cout << "[ERROR] [" << logger_ << "]: CancelCaptureImage failed." << std::endl;
      return ret;
    }
    std::cout << "[INFO] [" << logger_ << "]: CancelCaptureImage success." << std::endl;
  }
  return ret;
}
}  // namespace qrb_camera