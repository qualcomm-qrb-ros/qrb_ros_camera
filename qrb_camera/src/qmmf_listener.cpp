/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include "qrb_camera/qmmf_listener.hpp"

#include <iostream>

namespace qrb_camera
{
QMMFListener::QMMFListener(ImageCallback image_cb) : image_cb_(image_cb) {}

void QMMFListener::on_preview_frame(ICameraFrame * buffer)
{
  auto frame = convert_buffer_to_camera_frame(buffer);
  image_cb_(std::move(frame));
}

void QMMFListener::on_video_frame(ICameraFrame * buffer) {};

void QMMFListener::on_picture_frame(ICameraFrame * buffer) {};

void QMMFListener::on_error()
{
  std::cout << "[ERROR] [" << logger_ << "]: Frame error." << std::endl;
};

std::unique_ptr<CameraFrame> QMMFListener::convert_buffer_to_camera_frame(ICameraFrame * buffer)
{
  auto frame = std::make_unique<CameraFrame>();
  frame->data = buffer->data;
  frame->frame_id = buffer->index;
  frame->orientation = buffer->orientation;
  frame->stride = buffer->stride;
  frame->slice = buffer->slice;
  frame->height = buffer->height;
  frame->width = buffer->width;
  frame->format = buffer->format;
  frame->size = buffer->size;
  frame->fd = buffer->fd;
  frame->timestamp = buffer->timestamp;
  frame->stream_name = buffer->frame_source;
  return std::move(frame);
}

}  // namespace qrb_camera