/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef QRB_CAMERA__QCARCAM_LISTENER_HPP_
#define QRB_CAMERA__QCARCAM_LISTENER_HPP_

#include <functional>
#include <memory>
#include <string>

#include "qrb_camera/camera_interface.hpp"
#include "qrb_camera/qrb_camera_common.hpp"

namespace qrb_camera
{
using ImageCallback = std::function<void(std::unique_ptr<CameraFrame> frame)>;

// QCarCamListener: ICameraListener implementation for the QCarCam backend.
// Receives on_preview_frame() calls from QCarCamCamera and converts the
// ICameraFrame into a CameraFrame that is forwarded to the ROS node via
// the registered ImageCallback.
class QCarCamListener : public ICameraListener
{
public:
  explicit QCarCamListener(ImageCallback image_cb);

  // Called by QCarCamCamera::dispatch_frame() for every new frame
  void on_preview_frame(ICameraFrame * frame) override;

  void on_error() override;

private:
  std::unique_ptr<CameraFrame> convert(ICameraFrame * frame);

  ImageCallback image_cb_;
  std::string   logger_ = "QCarCamListener";
};

}  // namespace qrb_camera
#endif  // QRB_CAMERA__QCARCAM_LISTENER_HPP_
