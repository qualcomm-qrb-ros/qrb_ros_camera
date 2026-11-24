/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

// Event log shared by the fake backends and the test program (one copy per process: both
// link this library), so the test can check what happened inside a backend and in which
// order, e.g. that a device is destroyed before its backend.

#ifndef QRB_CAMERA_TEST__FAKE_EVENTS_HPP_
#define QRB_CAMERA_TEST__FAKE_EVENTS_HPP_

#include <string>
#include <vector>

namespace fake_events
{
void record(const std::string & event);
std::vector<std::string> take();  // returns and clears the log
}  // namespace fake_events

#endif  // QRB_CAMERA_TEST__FAKE_EVENTS_HPP_
