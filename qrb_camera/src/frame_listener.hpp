/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef QRB_CAMERA__FRAME_LISTENER_HPP_
#define QRB_CAMERA__FRAME_LISTENER_HPP_

#include <iostream>
#include <memory>
#include <utility>

#include "camera_interface/camera_interface.hpp"

namespace qrb_camera
{
// The one listener for every backend: copies the fields of an ICameraFrame into a
// CameraFrame and calls the consumer's callback (what QMMFListener and QCarCamListener
// used to do, identically). Lives in qrb_camera, not in a backend library.
class FrameListener : public ICameraListener
{
public:
  explicit FrameListener(ImageCallback image_cb) : image_cb_(std::move(image_cb)) {}

  void on_preview_frame(ICameraFrame * frame) override
  {
    if (frame == nullptr || !image_cb_) {
      return;
    }
    image_cb_(convert(*frame));
  }

  void on_error() override
  {
    std::cout << "[ERROR] [CameraManager]: camera error reported by the backend" << std::endl;
  }

  static std::unique_ptr<CameraFrame> convert(const ICameraFrame & in)
  {
    auto out = std::make_unique<CameraFrame>();
    out->stream_name = in.frame_source;
    out->frame_id = in.index;
    out->size = in.size;
    out->width = in.width;
    out->height = in.height;
    out->stride = in.stride;
    out->slice = in.slice;
    out->format = in.format;
    out->timestamp = static_cast<int64_t>(in.timestamp);
    out->fd = in.fd;
    out->data = in.data;
    out->orientation = in.orientation;
    return out;
  }

private:
  ImageCallback image_cb_;
};
}  // namespace qrb_camera

#endif  // QRB_CAMERA__FRAME_LISTENER_HPP_
