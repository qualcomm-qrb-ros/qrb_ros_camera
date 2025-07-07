/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef QRB_CAMERA__QMMF_LISTENER_HPP_
#define QRB_CAMERA__QMMF_LISTENER_HPP_

#include <functional>
#include <memory>

#include "qrb_camera/camera_interface.hpp"
#include "qrb_camera/qrb_camera_common.hpp"

namespace qrb_camera
{
using ImageCallback = std::function<void(std::unique_ptr<CameraFrame> frame)>;

class QMMFListener : public ICameraListener
{
public:
  explicit QMMFListener(ImageCallback image_cb);

  void on_preview_frame(ICameraFrame * buffer);

  void on_video_frame(ICameraFrame * buffer);

  void on_picture_frame(ICameraFrame * buffer);

  void on_error();

private:
  std::unique_ptr<CameraFrame> convert_buffer_to_camera_frame(ICameraFrame * buffer);

  ImageCallback image_cb_;
  std::string logger_ = "QMMFListener";
};
}  // namespace qrb_camera
#endif