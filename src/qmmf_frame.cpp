/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include "qrb_camera/qmmf_frame.hpp"

#include <iostream>

using namespace qmmf;
using namespace recorder;

namespace qrb_camera
{
QMMFFrame::~QMMFFrame()
{
  status_t ret = 0;
  // Return buffer back to recorder service.
  if (type_ == StreamType::Track) {
    ret = recorder_->ReturnTrackBuffer(this->session_id_, this->buffers_);
  } else if (type_ == StreamType::Snapshot) {
    ret = recorder_->ReturnImageCaptureBuffer(this->camera_id_, this->buffer_);
  }
  if (ret != 0) {
    std::cout << "[ERROR] [" << logger_ << "]: Return image buffer failed." << std::endl;
  }
}

uint32_t QMMFFrame::acquire_ref()
{
  std::unique_lock lock(refs_mutex_);
  refs_++;
  return refs_;
}

uint32_t QMMFFrame::release_ref()
{
  std::unique_lock lock(refs_mutex_);
  if (refs_ == 1) {
    cb_(this);
  }
  return refs_--;
}

void QMMFFrame::dispatch_snapshot_frame(std::vector<ICameraListener *> listeners,
    qmmf::recorder::Recorder * recorder,
    qmmf::BufferDescriptor & buffer,
    qmmf::BufferMeta & meta_buffer,
    qmmf::CameraMetadata & frameMetadata,
    uint32_t camera_id,
    uint32_t image_id,
    std::string & stream_name,
    uint32_t width,
    uint32_t height)
{
  if (buffer.size == 0) {
    std::cout << "[ERROR] [" << logger_ << "]: buffer is empty in dispatch_snapshot_frame."
              << std::endl;
    return;
  }
  recorder_ = recorder;
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
  data = (uint8_t *)buffer.data;
  size = buffer.size;
  fd = buffer.fd;
  frame_source = stream_name;
  if (meta_buffer.format == BufferFormat::kNV12) {
    this->format = "nv12";
  }
  acquire_ref();
  for (unsigned int i = 0; i < listeners.size(); i++) {
    type_ = StreamType::Snapshot;
    listeners[i]->on_picture_frame(static_cast<ICameraFrame *>(this));
  }
  release_ref();
}

void QMMFFrame::dispatch_track_frame(std::vector<ICameraListener *> listeners,
    qmmf::recorder::Recorder * recorder,
    std::vector<qmmf::BufferDescriptor> & buffers,
    std::vector<qmmf::BufferMeta> & meta_buffers,
    qmmf::CameraMetadata & frameMetadata,
    uint32_t camera_id,
    uint32_t frame_index,
    std::string & stream_name,
    qmmf::recorder::VideoFormat format,
    uint32_t width,
    uint32_t height)
{
  if (meta_buffers.size() == 0 || buffers.size() == 0) {
    std::cout << "[ERROR] [" << logger_ << "]: buffer is empty in dispatch_track_frame."
              << std::endl;
    return;
  }
  recorder_ = recorder;
  buffers_ = buffers;
  meta_buffers_ = meta_buffers;

  stride = meta_buffers.begin()->planes[0].stride;
  slice = meta_buffers.begin()->planes[0].scanline;
  this->camera_id = camera_id;
  this->width = width;
  this->height = height;
  buf_id_ = buffers.begin()->buf_id;
  timestamp = buffers.begin()->timestamp;
  data = (uint8_t *)buffers.begin()->data;
  size = buffers.begin()->size;
  fd = buffers.begin()->fd;
  frame_source = stream_name;
  index = frame_index;
  if (meta_buffers.begin()->format == BufferFormat::kNV12) {
    this->format = "nv12";
  } else if (meta_buffers.begin()->format == BufferFormat::kRAW16) {
    this->format = "y16";
  } else if (meta_buffers.begin()->format == BufferFormat::kBLOB) {
    this->format = "jpeg";
  } else {
    std::cout << "[WARNING] [" << logger_ << "]: unknown buffer format." << std::endl;
    this->format = "unknown";
  }

  acquire_ref();
  for (unsigned int i = 0; i < listeners.size(); i++) {
    type_ = StreamType::Track;
    listeners[i]->on_preview_frame(static_cast<ICameraFrame *>(this));
  }
  release_ref();
}
}  // namespace qrb_camera