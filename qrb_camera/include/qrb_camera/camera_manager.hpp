/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef QRB_CAMERA__CAMERA_MANAGER_HPP_
#define QRB_CAMERA__CAMERA_MANAGER_HPP_

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "camera_interface/camera_interface.hpp"
#include "camera_interface/camera_types.hpp"
#include "plugin_discovery/plugin_registry.hpp"

namespace qrb_camera
{
// A camera that a loaded backend reported.
struct CameraEntry
{
  std::string backend;  // backend name, e.g. "qcarcam"
  std::string id;       // the backend's camera id, e.g. "11"
  std::string description;
};

class FrameListener;

/**
 * Loads the camera backends through plugin_discovery and manages the cameras opened on
 * them. Knows no backend by name and never looks at the SoC: the configuration lists the
 * backends, each backend's probe() reports its cameras.
 *
 * Not copyable. All methods are thread-safe.
 */
class CameraManager
{
public:
  /**
   * Scans the manifests and the configuration and loads the camera backends once.
   *
   * @param manual_backends Load only these backend names, replacing the configuration
   *        (as PLUGIN_DISCOVERY_BACKEND_CAMERA does). Empty: the configuration decides.
   * @param plugin_paths Directories with the backends' *.plugin.yaml manifests. Empty:
   *        $PLUGIN_DISCOVERY_PATH if set, else the installed default directory plus
   *        <prefix>/share/camera/plugins of every prefix in $AMENT_PREFIX_PATH and
   *        $CMAKE_PREFIX_PATH, and of every install base in $COLCON_PREFIX_PATH and every
   *        package prefix below it (finds backends installed into separate prefixes by colcon).
   */
  explicit CameraManager(std::vector<std::string> manual_backends = {},
      std::vector<std::string> plugin_paths = {});
  ~CameraManager();

  CameraManager(const CameraManager &) = delete;
  CameraManager & operator=(const CameraManager &) = delete;

  /// Cameras reported by the loaded backends, in load order.
  std::vector<CameraEntry> list_cameras() const;

  /// Backend name of the default (the first entry of the configured list); empty if none.
  /// The default may be configured but not loaded: create_camera() then fails.
  std::string default_backend() const;

  /// Names of the loaded backends, in load order.
  std::vector<std::string> loaded_backends() const;

  /**
   * Opens a camera.
   *
   * @param camera_id The backend's camera id (QMMF camera id, QCarCam inputId).
   * @param backend Backend name. Empty: the default backend. No fallback to another one.
   * @return index for the other methods, or -1 on failure (reason logged).
   */
  int create_camera(uint32_t camera_id, const std::string & backend = "");

  bool set_camera_parameter(int index, CameraConfigure & param);
  bool register_callback(int index, ImageCallback image_cb);

  /// set_param -> init -> start_camera on the device. False if any step fails.
  bool start_camera(int index);
  /// Removes the registered callbacks and stops the device.
  void stop_camera(int index);
  bool destroy_camera(int index);

  /// Backend name of a created camera; empty for an invalid index.
  std::string camera_backend(int index) const;

  /// ICameraDevice::now_ns() of a created camera (the clock of its frame timestamps);
  /// -1 for an invalid index.
  int64_t device_now_ns(int index) const;

private:
  struct Slot;
  using Loaded = plugin_discovery::LoadedBackend<ICameraBackend>;

  const Loaded * find_loaded(const std::string & backend, std::string & error) const;
  Slot * slot(int index, const char * what);
  const Slot * slot(int index, const char * what) const;
  void stop_locked(Slot & s);
  void destroy_locked(Slot & s);
  void print_summary() const;

  mutable std::mutex mtx_;
  // Declaration order is destruction order in reverse: the cameras (slots_) go first, then
  // the backend objects and their libraries (backends_), then the registry.
  plugin_discovery::PluginRegistry registry_;
  plugin_discovery::BackendSet<ICameraBackend> backends_;
  std::vector<std::unique_ptr<Slot>> slots_;
};
}  // namespace qrb_camera

#endif  // QRB_CAMERA__CAMERA_MANAGER_HPP_
