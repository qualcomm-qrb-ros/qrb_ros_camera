/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

// A camera backend without a vendor SDK, built once per FAKE_CLASS (see test/CMakeLists.txt).
// Its behaviour is driven by environment variables read at the time of each call, so one
// library serves every test case:
//   FAKE_CAMERA_<UPPER>_PROBE    ok | unsupported | fault | throw   (default ok)
//   FAKE_CAMERA_<UPPER>_DEVICES  comma-separated camera ids          (default FAKE_DEVICES)
//   FAKE_CAMERA_<UPPER>_OPEN     ok | null | throw                   (default ok)
//   FAKE_CAMERA_<UPPER>_INIT     ok | fail                           (default ok)
//   FAKE_CAMERA_<UPPER>_START    ok | fail                           (default ok)
// start_camera() delivers one frame to the listeners synchronously.

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "camera_interface/camera_interface.hpp"
#include "fake_events.hpp"

#if !defined(FAKE_CLASS) || !defined(FAKE_BACKEND) || !defined(FAKE_UPPER) || !defined(FAKE_DEVICES)
#error "FAKE_CLASS, FAKE_BACKEND, FAKE_UPPER and FAKE_DEVICES must be defined"
#endif

namespace
{
std::string knob(const char * what, const char * fallback)
{
  const std::string var = std::string("FAKE_CAMERA_") + FAKE_UPPER + "_" + what;
  const char * value = std::getenv(var.c_str());
  return value != nullptr ? value : fallback;
}

void record(const std::string & what)
{
  fake_events::record(std::string(FAKE_BACKEND) + " " + what);
}

// Internal linkage: every fake backend library has its own Frame and Device.
class Frame : public qrb_camera::ICameraFrame
{
public:
  uint32_t acquire_ref() override { return ++refs_; }
  uint32_t release_ref() override { return refs_ > 0 ? --refs_ : 0; }
};

class Device : public qrb_camera::ICameraDevice
{
public:
  explicit Device(uint32_t id) : ICameraDevice(id) { record("device ctor " + std::to_string(id)); }
  ~Device() override { record("device dtor " + std::to_string(camera_id_)); }

  void add_listener(qrb_camera::ICameraListener * listener) override
  {
    listeners_.push_back(listener);
  }

  void remove_listener(qrb_camera::ICameraListener * listener) override
  {
    listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), listener), listeners_.end());
  }

  void set_param(std::vector<qrb_camera::CameraParam> & param) override { param_ = param; }

  bool init() override
  {
    record("init " + std::to_string(camera_id_));
    return knob("INIT", "ok") == "ok";
  }

  int start_camera() override
  {
    if (knob("START", "ok") != "ok") {
      return -1;
    }
    is_working_ = true;
    record("start " + std::to_string(camera_id_));

    Frame frame;
    frame.timestamp = static_cast<uint64_t>(now_ns() - 100);
    frame.index = 1;
    frame.format = "nv12";
    frame.camera_id = camera_id_;
    if (!param_.empty() && !param_[0].track_params.empty()) {
      const auto & track = param_[0].track_params[0];
      frame.frame_source = track.name;
      frame.width = track.width;
      frame.height = track.height;
    }
    for (auto * listener : listeners_) {
      listener->on_preview_frame(&frame);
    }
    return 0;
  }

  int stop_camera() override
  {
    is_working_ = false;
    record("stop " + std::to_string(camera_id_));
    return 0;
  }

  int64_t now_ns() const override { return 1000000 + camera_id_; }

private:
  std::vector<qrb_camera::ICameraListener *> listeners_;
  std::vector<qrb_camera::CameraParam> param_;
};
}  // namespace

namespace fake_camera
{
class FAKE_CLASS : public qrb_camera::ICameraBackend
{
public:
  FAKE_CLASS() { record("backend ctor"); }
  ~FAKE_CLASS() override { record("backend dtor"); }

  std::string name() const override { return FAKE_BACKEND; }

  plugin_discovery::ProbeResult probe() override
  {
    const std::string mode = knob("PROBE", "ok");
    record("probe " + mode);
    if (mode == "unsupported") {
      return plugin_discovery::ProbeResult::unsupported("fake: not this machine");
    }
    if (mode == "fault") {
      return plugin_discovery::ProbeResult::fault("fake: device error");
    }
    if (mode == "throw") {
      throw std::runtime_error("fake: probe threw");
    }
    std::vector<plugin_discovery::DeviceInfo> devices;
    std::stringstream ss(knob("DEVICES", FAKE_DEVICES));
    std::string id;
    while (std::getline(ss, id, ',')) {
      if (!id.empty()) {
        devices.push_back({id, "", std::string("fake camera ") + id});
      }
    }
    return plugin_discovery::ProbeResult::ok(std::move(devices));
  }

  std::unique_ptr<qrb_camera::ICameraDevice> open(
      const plugin_discovery::DeviceInfo & device) override
  {
    const std::string mode = knob("OPEN", "ok");
    if (mode == "throw") {
      throw std::runtime_error("fake: open threw");
    }
    if (mode == "null") {
      return nullptr;
    }
    return std::make_unique<Device>(static_cast<uint32_t>(std::stoul(device.id)));
  }
};
}  // namespace fake_camera

PLUGIN_DISCOVERY_REGISTER_BACKEND(fake_camera::FAKE_CLASS, qrb_camera::ICameraBackend)
