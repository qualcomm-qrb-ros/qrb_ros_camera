/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef CAMERA_BACKEND_QCARCAM__QTIMER_HPP_
#define CAMERA_BACKEND_QCARCAM__QTIMER_HPP_

#include <cstdint>

namespace qrb_camera
{
// Current QTimer (ARM generic timer) time in ns: the clock of QCarCam frame timestamps.
// Moved here from the IQ10 qrb_ros_camera CameraNode::getQtimerTime().
inline uint64_t qtimer_now_ns()
{
  uint64_t count = 0;
  uint64_t freq = 0;
#if defined(__aarch64__)
  asm volatile("mrs %0, cntvct_el0" : "=r"(count));
  asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
#elif defined(__arm__)
  asm volatile("mrrc p15, 1, %Q0, %R0, c14" : "=r"(count));
  freq = 19200000;  // 19.2 MHz
#else
#error "QTimer is only available on ARM"
#endif
  if (freq == 0) {
    return 0;
  }
  const uint64_t sec = count / freq;
  const uint64_t rem = count % freq;
  return sec * 1000000000ULL + rem * 1000000000ULL / freq;
}
}  // namespace qrb_camera

#endif  // CAMERA_BACKEND_QCARCAM__QTIMER_HPP_
