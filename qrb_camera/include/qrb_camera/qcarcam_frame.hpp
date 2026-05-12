/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef QRB_CAMERA__QCARCAM_FRAME_HPP_
#define QRB_CAMERA__QCARCAM_FRAME_HPP_

#include <functional>
#include <mutex>
#include <string>

#include "qrb_camera/camera_interface.hpp"
#include "qrb_camera/qrb_camera_common.hpp"

namespace qrb_camera
{

// QCarCamFrame: lightweight ICameraFrame wrapper for a single QCarCam buffer.
//
// Lifecycle:
//   1. QCarCamCamera::dispatch_frame() creates a QCarCamFrame on the stack
//      (or heap) and fills its fields from the GBM buffer.
//   2. The frame is passed to each ICameraListener::on_preview_frame().
//   3. The listener (QCarCamListener) converts it to CameraFrame and calls
//      the ROS image callback.
//   4. After all listeners have been notified the frame object is destroyed.
//      Buffer release back to QCarCam is handled separately by
//      QCarCamCamera::resubmit_buffer().
//
// Note: QCarCamFrame does NOT own the underlying memory – it only holds a
// pointer/fd into the GBM buffer that is managed by QCarCamCamera.
class QCarCamFrame : public ICameraFrame
{
public:
  QCarCamFrame() = default;
  ~QCarCamFrame() override = default;

  // ICameraFrame reference counting (simple, not thread-safe – frames are
  // used synchronously within dispatch_frame so no concurrent access occurs)
  uint32_t acquire_ref() override { return ++refs_; }
  uint32_t release_ref() override { return --refs_; }
};

}  // namespace qrb_camera
#endif  // QRB_CAMERA__QCARCAM_FRAME_HPP_
