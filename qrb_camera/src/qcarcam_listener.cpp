/*
 *​​​​ Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include "qrb_camera/qcarcam_listener.hpp"

#include <iostream>

namespace qrb_camera
{

QCarCamListener::QCarCamListener(ImageCallback image_cb, EventCallback event_cb)
  : image_cb_(image_cb), event_cb_(event_cb)
{
}

void QCarCamListener::on_preview_frame(ICameraFrame * frame)
{
  if (!frame) {
    std::cout << "[ERROR] [" << logger_ << "]: on_preview_frame: null frame" << std::endl;
    return;
  }
  auto camera_frame = convert(frame);
  image_cb_(std::move(camera_frame));
}

void QCarCamListener::on_event(const CameraEvent & event)
{
  if (event_cb_) {
    event_cb_(event);
  }
}

std::unique_ptr<CameraFrame> QCarCamListener::convert(ICameraFrame * frame)
{
  auto cf = std::make_unique<CameraFrame>();
  cf->data = frame->data;
  cf->fd = frame->fd;
  cf->size = frame->size;
  cf->width = frame->width;
  cf->height = frame->height;
  cf->stride = frame->stride;
  cf->slice = frame->slice;
  cf->format = frame->format;
  cf->timestamp = static_cast<int64_t>(frame->timestamp);
  cf->frame_id = frame->index;
  cf->orientation = frame->orientation;
  cf->stream_name = frame->frame_source;
  return cf;
}

}  // namespace qrb_camera
