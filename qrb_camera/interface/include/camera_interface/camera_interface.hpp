/*
 * Copyright (c) 2015, 2017, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

// The camera domain interface of plugin_discovery.
//
// Every camera backend (one per vendor SDK: QMMF/CamX, QCarCam, ...) implements
// ICameraBackend and registers it with
//   PLUGIN_DISCOVERY_REGISTER_BACKEND(<Derived>, qrb_camera::ICameraBackend)
// plugin_discovery loads the backends the configuration lists, calls probe() on each, and
// qrb_camera::CameraManager opens an ICameraDevice on one of them.

#ifndef CAMERA_INTERFACE__CAMERA_INTERFACE_HPP_
#define CAMERA_INTERFACE__CAMERA_INTERFACE_HPP_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "camera_interface/camera_types.hpp"
#include "plugin_discovery/backend.hpp"

namespace qrb_camera
{
/**
 * One image frame produced by a backend. Valid only during the listener callback,
 * unless the backend gives acquire_ref() a meaning:
 *  - QMMF: acquire_ref() inside the callback keeps the buffer until release_ref();
 *  - QCarCam: the buffer goes back to the driver when the callback returns, whatever the
 *    reference count says.
 * Portable code copies (or dup()s the fd) inside the callback.
 */
class ICameraFrame
{
  ICameraFrame(const ICameraFrame &) = delete;
  void operator=(const ICameraFrame &) = delete;

protected:
  uint32_t refs_;
  ICameraFrame() : refs_(0), timestamp(0), data(nullptr), fd(-1) {}
  virtual ~ICameraFrame() {}

public:
  /// Take a reference to the frame; see the class comment for what it guarantees.
  virtual uint32_t acquire_ref() = 0;
  /// Drop a reference taken with acquire_ref().
  virtual uint32_t release_ref() = 0;

  uint64_t timestamp;  ///< ns, in the clock of ICameraDevice::now_ns()
  uint32_t size = 0;   ///< data size in bytes
  uint8_t * data;      ///< start of the image data
  int fd;              ///< dma-buf fd of the frame memory, -1 if not shared
  void * metadata = nullptr;
  uint32_t stride = 0;
  uint32_t slice = 0;
  std::string frame_source;  ///< stream name
  std::string format;        ///< "nv12", "nv21", "uyvy", "y16", "jpeg", ...
  int orientation = 0;       ///< raw10 only: 0 left camera, 1 right camera
  uint32_t camera_id = 0;
  uint32_t index = 0;  ///< frame counter
  uint32_t width = 0;
  uint32_t height = 0;
};

/**
 * Receives frames and events from a camera device. The methods may be called from
 * threads of the vendor SDK; implementations must be thread-safe.
 */
class ICameraListener
{
public:
  ICameraListener() {}
  virtual ~ICameraListener() {}

  virtual void on_error() {}
  virtual void on_preview_frame(ICameraFrame * frame) { (void)frame; }
  virtual void on_video_frame(ICameraFrame * frame) { (void)frame; }
  virtual void on_picture_frame(ICameraFrame * frame) { (void)frame; }
  virtual void on_metadata_frame(ICameraFrame * frame) { (void)frame; }
};

/**
 * One camera, created by ICameraBackend::open() for a device the backend reported.
 * Owned by the caller; it must be destroyed before the backend that created it.
 *
 * Call order: add_listener() ... set_param() -> init() -> start_camera() ... stop_camera().
 */
class ICameraDevice
{
  ICameraDevice(const ICameraDevice &) = delete;
  void operator=(const ICameraDevice &) = delete;

public:
  explicit ICameraDevice(uint32_t id) : camera_id_(id) {}
  virtual ~ICameraDevice() {}

  /// The listener must stay valid until remove_listener() or the device is destroyed.
  virtual void add_listener(ICameraListener * listener) = 0;
  virtual void remove_listener(ICameraListener * listener) = 0;

  virtual void set_param(std::vector<CameraParam> & param) = 0;
  /// Prepare the device with the parameters set; false on failure.
  virtual bool init() = 0;
  /// 0 on success.
  virtual int start_camera() = 0;
  /// 0 on success.
  virtual int stop_camera() = 0;

  /**
   * The current time in the clock that frame timestamps use, in ns
   * (QMMF: CLOCK_BOOTTIME, QCarCam: QTimer). Consumers compute the offset to their own
   * clock from it, without knowing which backend this is.
   */
  virtual int64_t now_ns() const = 0;

  bool working() const { return is_working_; }
  uint32_t get_id() const { return camera_id_; }

protected:
  bool is_working_ = false;
  uint32_t camera_id_;
};

/**
 * The plugin each camera backend registers with plugin_discovery.
 *
 * probe() (from plugin_discovery::Backend) asks the vendor SDK whether it works on this
 * machine and which cameras it can serve: DeviceInfo::id is the camera id as a decimal
 * string (QMMF camera id, QCarCam inputId). Unsupported / Fault must leave nothing behind.
 * A backend whose SDK cannot enumerate cameras declares the capability "unlisted_ids";
 * then any camera id may be passed to open().
 */
class ICameraBackend : public plugin_discovery::Backend
{
public:
  /// The backend name, e.g. "qmmf"; the same as `backend:` in its manifest.
  virtual std::string name() const = 0;

  /// A new device for `device` (one probe() reported, or any id with "unlisted_ids").
  /// nullptr or an exception on failure.
  virtual std::unique_ptr<ICameraDevice> open(const plugin_discovery::DeviceInfo & device) = 0;
};

/// Capability of a backend that cannot enumerate its cameras (see ICameraBackend).
inline constexpr const char * kCapabilityUnlistedIds = "unlisted_ids";
}  // namespace qrb_camera

PLUGIN_DISCOVERY_INTERFACE(qrb_camera::ICameraBackend, "camera", 1, 0)

#endif  // CAMERA_INTERFACE__CAMERA_INTERFACE_HPP_
