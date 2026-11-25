/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "qmmf_frame.hpp"

#include <iostream>

using namespace qmmf;
using namespace recorder;

namespace qrb_camera
{
QMMFFrame::~QMMFFrame()
{
  if (recorder_ == nullptr) {
    return;  // no buffer attached
  }
  status_t ret = 0;
  // Return the buffer to the recorder service.
  if (type_ == StreamType::Track) {
    ret = recorder_->ReturnTrackBuffer(session_id_, buffers_);
  } else if (type_ == StreamType::Snapshot) {
    ret = recorder_->ReturnImageCaptureBuffer(camera_id_, buffer_);
  }
  if (ret != 0) {
    std::cout << "[ERROR] [" << logger_ << "]: Return image buffer failed." << std::endl;
  }
}

uint32_t QMMFFrame::acquire_ref()
{
  std::lock_guard<std::mutex> lock(refs_mutex_);
  return ++refs_;
}

uint32_t QMMFFrame::release_ref()
{
  // Fix of the original code, which called cb_ (it deletes this frame) while holding
  // refs_mutex_ and then decremented refs_: both touched freed memory. Decide under the
  // lock, delete after releasing it.
  uint32_t left = 0;
  {
    std::lock_guard<std::mutex> lock(refs_mutex_);
    if (refs_ == 0) {
      return 0;
    }
    left = --refs_;
  }
  if (left == 0) {
    cb_(this);  // removes and deletes this frame, which returns the buffer
  }
  return left;
}

void QMMFFrame::dispatch_snapshot_frame(std::vector<ICameraListener *> & listeners,
    qmmf::recorder::Recorder * recorder,
    qmmf::BufferDescriptor & buffer,
    qmmf::BufferMeta & meta_buffer,
    uint32_t camera_id,
    uint32_t image_id,
    std::string & stream_name,
    uint32_t width,
    uint32_t height)
{
  recorder_ = recorder;
  type_ = StreamType::Snapshot;
  buffer_ = buffer;
  meta_buffer_ = meta_buffer;

  stride = meta_buffer.planes[0].stride;
  slice = meta_buffer.planes[0].scanline;
  this->camera_id = camera_id;
  this->width = width;
  this->height = height;
  session_id_ = image_id;
  buf_id_ = buffer.buf_id;
  timestamp = buffer.timestamp;
  data = (uint8_t *)buffer.data;  // as in the original: the SDK field type is not ours
  size = buffer.size;
  fd = buffer.fd;
  frame_source = stream_name;
  if (meta_buffer.format == BufferFormat::kNV12) {
    this->format = "nv12";
  }

  acquire_ref();
  for (auto * listener : listeners) {
    listener->on_picture_frame(static_cast<ICameraFrame *>(this));
  }
  release_ref();  // may delete this frame
}

void QMMFFrame::dispatch_track_frame(std::vector<ICameraListener *> & listeners,
    qmmf::recorder::Recorder * recorder,
    std::vector<qmmf::BufferDescriptor> & buffers,
    std::vector<qmmf::BufferMeta> & meta_buffers,
    uint32_t camera_id,
    uint32_t frame_index,
    std::string & stream_name,
    uint32_t width,
    uint32_t height)
{
  recorder_ = recorder;
  type_ = StreamType::Track;
  buffers_ = buffers;
  meta_buffers_ = meta_buffers;

  stride = meta_buffers.front().planes[0].stride;
  slice = meta_buffers.front().planes[0].scanline;
  this->camera_id = camera_id;
  this->width = width;
  this->height = height;
  buf_id_ = buffers.front().buf_id;
  timestamp = buffers.front().timestamp;
  data = (uint8_t *)buffers.front().data;  // as in the original
  size = buffers.front().size;
  fd = buffers.front().fd;
  frame_source = stream_name;
  index = frame_index;
  if (meta_buffers.front().format == BufferFormat::kNV12) {
    this->format = "nv12";
  } else if (meta_buffers.front().format == BufferFormat::kRAW16) {
    this->format = "y16";
  } else if (meta_buffers.front().format == BufferFormat::kBLOB) {
    this->format = "jpeg";
  } else {
    std::cout << "[WARNING] [" << logger_ << "]: unknown buffer format." << std::endl;
    this->format = "unknown";
  }

  acquire_ref();
  for (auto * listener : listeners) {
    listener->on_preview_frame(static_cast<ICameraFrame *>(this));
  }
  release_ref();  // may delete this frame
}
}  // namespace qrb_camera
