/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "fake_events.hpp"

#include <mutex>
#include <utility>

namespace fake_events
{
namespace
{
std::mutex g_mutex;
std::vector<std::string> g_events;
}  // namespace

void record(const std::string & event)
{
  std::lock_guard<std::mutex> lock(g_mutex);
  g_events.push_back(event);
}

std::vector<std::string> take()
{
  std::lock_guard<std::mutex> lock(g_mutex);
  return std::exchange(g_events, {});
}
}  // namespace fake_events
