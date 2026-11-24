/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef CAMERA_BACKEND_QCARCAM__QCARCAM_FRAME_HPP_
#define CAMERA_BACKEND_QCARCAM__QCARCAM_FRAME_HPP_

#include "camera_interface/camera_interface.hpp"

namespace qrb_camera
{
// A view of one QCarCam buffer, built on the stack for one dispatch. Does not own the
// memory. The reference count is not used: the buffer goes back to the driver when the
// listeners return (QCarCamStream release queue), whatever acquire_ref() says. This is the
// original IQ10 behaviour, kept on purpose (see DESIGN.md, known issue 1).
class QCarCamFrame : public ICameraFrame
{
public:
  QCarCamFrame() = default;
  ~QCarCamFrame() override = default;

  uint32_t acquire_ref() override { return ++refs_; }
  uint32_t release_ref() override { return refs_ > 0 ? --refs_ : 0; }
};
}  // namespace qrb_camera

#endif  // CAMERA_BACKEND_QCARCAM__QCARCAM_FRAME_HPP_
