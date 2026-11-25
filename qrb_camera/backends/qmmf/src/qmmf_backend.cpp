/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

// The QMMF/CamX camera backend registered with plugin_discovery.

#include <qmmf-sdk/qmmf_recorder.h>

#include <memory>
#include <string>

#include "camera_interface/camera_interface.hpp"
#include "qmmf_camera.hpp"

namespace qrb_camera
{
class QMMFBackend : public ICameraBackend
{
public:
  std::string name() const override { return "qmmf"; }

  // Connects to the QMMF recorder service once and disconnects again: no service, no QMMF
  // here. The SDK has no camera enumeration we use, so camera 0 is reported and the
  // manifest declares "unlisted_ids": any camera id may be opened; a wrong one fails in
  // start_camera().
  plugin_discovery::ProbeResult probe() override
  {
    qmmf::recorder::Recorder recorder;
    qmmf::recorder::RecorderCb cb;
    cb.event_cb = [](qmmf::recorder::EventType, void *, size_t) {};
    if (recorder.Connect(cb) != 0) {
      return plugin_discovery::ProbeResult::unsupported(
          "QMMF recorder service not available (no QMMF/CamX on this machine)");
    }
    recorder.Disconnect();
    return plugin_discovery::ProbeResult::ok({{"0", "", "QMMF camera 0"}});
  }

  std::unique_ptr<ICameraDevice> open(const plugin_discovery::DeviceInfo & device) override
  {
    return std::make_unique<QMMFCamera>(static_cast<uint32_t>(std::stoul(device.id)));
  }
};
}  // namespace qrb_camera

PLUGIN_DISCOVERY_REGISTER_BACKEND(qrb_camera::QMMFBackend, qrb_camera::ICameraBackend)
