/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

// The QCarCam camera backend registered with plugin_discovery.
//
// Owns the QCarCam SDK session of its cameras (DESIGN.md 4.3): QCarCamInitialize in probe(),
// kept while the backend is loaded, QCarCamUninitialize in the destructor. plugin_discovery
// and CameraManager destroy every camera before the backend, so no camera uses the session
// after it ends. Cameras never call QCarCamInitialize / QCarCamUninitialize themselves.

#include <qcarcam.h>

#include <memory>
#include <string>
#include <vector>

#include "camera_interface/camera_interface.hpp"
#include "qcarcam_camera.hpp"

namespace qrb_camera
{
class QCarCamBackend : public ICameraBackend
{
public:
  ~QCarCamBackend() override
  {
    if (initialized_) {
      QCarCamUninitialize();
    }
  }

  std::string name() const override { return "qcarcam"; }

  // Initializes QCarCam and reports its inputs (camera id = inputId). No waiting or retry:
  // if the QCX server is not ready yet (node started before it), this reports Fault
  // (DESIGN.md, known issue 5).
  // qcarcam is only loaded where the configuration lists it, so a failure here is a fault
  // of this machine, not "unsupported".
  plugin_discovery::ProbeResult probe() override
  {
    QCarCamInit_t init = {};
    init.apiVersion = QCARCAM_VERSION;
    const QCarCamRet_e ret = QCarCamInitialize(&init);
    if (ret != QCARCAM_RET_OK) {
      return plugin_discovery::ProbeResult::fault("QCarCamInitialize failed (" +
                                                  std::to_string(static_cast<int>(ret)) +
                                                  "): the QCX server is not running or not ready "
                                                  "yet");
    }
    initialized_ = true;

    std::vector<plugin_discovery::DeviceInfo> devices;
    uint32_t count = 0;
    if (QCarCamQueryInputs(nullptr, 0, &count) == QCARCAM_RET_OK && count > 0) {
      std::vector<QCarCamInput_t> inputs(count);
      uint32_t filled = 0;
      if (QCarCamQueryInputs(inputs.data(), count, &filled) == QCARCAM_RET_OK) {
        for (uint32_t i = 0; i < filled && i < count; ++i) {
          const std::string id = std::to_string(inputs[i].inputId);
          devices.push_back({id, "", "QCarCam input " + id});
        }
      }
    }
    if (devices.empty()) {
      // Fault: leave nothing behind.
      QCarCamUninitialize();
      initialized_ = false;
      return plugin_discovery::ProbeResult::fault("QCarCam runs here but reports no input");
    }
    return plugin_discovery::ProbeResult::ok(std::move(devices));
  }

  std::unique_ptr<ICameraDevice> open(const plugin_discovery::DeviceInfo & device) override
  {
    if (!initialized_) {
      return nullptr;
    }
    return std::make_unique<QCarCamCamera>(static_cast<uint32_t>(std::stoul(device.id)));
  }

private:
  bool initialized_ = false;
};
}  // namespace qrb_camera

PLUGIN_DISCOVERY_REGISTER_BACKEND(qrb_camera::QCarCamBackend, qrb_camera::ICameraBackend)
