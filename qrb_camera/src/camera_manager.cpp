/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "qrb_camera/camera_manager.hpp"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <utility>

#include "frame_listener.hpp"

#ifndef QRB_CAMERA_PLUGIN_DIR
#error "QRB_CAMERA_PLUGIN_DIR must be defined by the build (plugin_discovery_plugin_dir)"
#endif

namespace qrb_camera
{
namespace
{
namespace fs = std::filesystem;

void log(const char * level, const std::string & msg)
{
  std::cout << "[" << level << "] [CameraManager]: " << msg << std::endl;
}

std::string join(const std::vector<std::string> & items)
{
  std::string out;
  for (const auto & item : items) {
    out += (out.empty() ? "" : ", ") + item;
  }
  return out;
}

std::vector<std::string> split_paths(const char * value)
{
  std::vector<std::string> out;
  if (value == nullptr) {
    return out;
  }
  std::stringstream ss(value);
  std::string item;
  while (std::getline(ss, item, ':')) {
    if (!item.empty()) {
      out.push_back(item);
    }
  }
  return out;
}

// See the constructor documentation in camera_manager.hpp.
std::vector<std::string> resolve_plugin_paths(std::vector<std::string> explicit_paths)
{
  if (!explicit_paths.empty()) {
    return explicit_paths;
  }
  const char * env = std::getenv("PLUGIN_DISCOVERY_PATH");
  if (env != nullptr && *env != '\0') {
    return {};  // PluginRegistry reads $PLUGIN_DISCOVERY_PATH itself
  }

  const fs::path rel = fs::path("share") / "camera" / "plugins";
  std::vector<std::string> candidates{QRB_CAMERA_PLUGIN_DIR};
  for (const char * var : {"AMENT_PREFIX_PATH", "CMAKE_PREFIX_PATH"}) {
    for (const auto & prefix : split_paths(std::getenv(var))) {
      candidates.push_back((fs::path(prefix) / rel).string());
    }
  }
  // colcon workspaces: $COLCON_PREFIX_PATH holds the install bases. A merged install has
  // <base>/share/camera/plugins; the default isolated install has one prefix per package,
  // <base>/<package>/share/camera/plugins, and colcon does not put a plain CMake package
  // without a CMake config file (like a backend) on any other path variable.
  for (const auto & base : split_paths(std::getenv("COLCON_PREFIX_PATH"))) {
    candidates.push_back((fs::path(base) / rel).string());
    std::error_code ec;
    std::vector<std::string> packages;
    for (fs::directory_iterator it(base, ec), end; !ec && it != end; it.increment(ec)) {
      packages.push_back((it->path() / rel).string());
    }
    std::sort(packages.begin(), packages.end());
    candidates.insert(candidates.end(), packages.begin(), packages.end());
  }

  std::vector<std::string> out;
  std::vector<fs::path> seen;
  for (const auto & dir : candidates) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
      continue;
    }
    const fs::path key = fs::weakly_canonical(dir, ec);
    if (std::find(seen.begin(), seen.end(), key) != seen.end()) {
      continue;
    }
    seen.push_back(key);
    out.push_back(dir);
  }
  if (out.empty()) {
    out.push_back(QRB_CAMERA_PLUGIN_DIR);  // nothing installed: report against the default
  }
  return out;
}

std::string backend_name(const plugin_discovery::PluginInfo & info)
{
  return info.backend.empty() ? info.name : info.backend;
}

bool has_capability(const plugin_discovery::PluginInfo & info, const char * cap)
{
  return std::find(info.capabilities.begin(), info.capabilities.end(), cap) !=
         info.capabilities.end();
}

void convert_param(const CameraConfigure & in, std::vector<CameraParam> & out)
{
  CameraParam camera_param;
  const size_t n = std::min<size_t>({static_cast<size_t>(std::max(in.stream_size, 0)),
      in.stream_type.size(), in.stream_widths.size(), in.stream_heights.size(),
      in.stream_fps.size(), in.stream_formats.size(), in.stream_names.size()});
  if (n < static_cast<size_t>(std::max(in.stream_size, 0))) {
    log("WARNING", "stream_size is " + std::to_string(in.stream_size) + " but only " +
                       std::to_string(n) + " stream(s) are fully described; using " +
                       std::to_string(n));
  }
  for (size_t i = 0; i < n; ++i) {
    StreamParam stream;
    stream.width = in.stream_widths[i];
    stream.height = in.stream_heights[i];
    stream.frame_rate = in.stream_fps[i];
    stream.format = in.stream_formats[i];
    stream.name = in.stream_names[i];
    if (in.stream_type[i] == StreamType::Track) {
      camera_param.track_params.push_back(stream);
    } else if (in.stream_type[i] == StreamType::Snapshot) {
      camera_param.snapshot_params.push_back(stream);
    }
  }
  out.push_back(camera_param);
}
}  // namespace

struct CameraManager::Slot
{
  std::string backend;
  uint32_t id = 0;
  std::shared_ptr<CameraConfigure> param;
  // Declared before `device`: if a Slot is ever destroyed member-wise, the device goes
  // first and never sees a dangling listener.
  std::vector<std::unique_ptr<FrameListener>> listeners;
  std::unique_ptr<ICameraDevice> device;
};

CameraManager::CameraManager(std::vector<std::string> manual_backends,
    std::vector<std::string> plugin_paths)
  : registry_(resolve_plugin_paths(std::move(plugin_paths)))
{
  try {
    registry_.scan();
    backends_ = registry_.load<ICameraBackend>(manual_backends);
  } catch (const std::exception & e) {
    log("ERROR", std::string("loading the camera backends failed: ") + e.what());
    backends_ = {};
  }
  for (const auto & b : backends_.backends) {
    const std::string configured = backend_name(b.handle.info());
    const std::string reported = b.handle->name();
    if (reported != configured) {
      log("WARNING", "backend " + b.handle.info().name + " calls itself '" + reported +
                         "' but its manifest says '" + configured + "'; the manifest name is used");
    }
  }
  print_summary();
}

CameraManager::~CameraManager()
{
  std::lock_guard<std::mutex> lock(mtx_);
  for (auto & s : slots_) {
    destroy_locked(*s);
  }
  slots_.clear();  // before backends_ (member order), stated for clarity
}

void CameraManager::print_summary() const
{
  std::ostringstream os;
  os << "camera backends (" << (backends_.how.empty() ? "none" : backends_.how) << "): ";
  if (backends_.backends.empty()) {
    os << "none loaded";
  }
  for (size_t i = 0; i < backends_.backends.size(); ++i) {
    const auto & b = backends_.backends[i];
    std::vector<std::string> ids;
    for (const auto & d : b.devices) {
      ids.push_back(d.id);
    }
    os << (i ? "; " : "") << backend_name(b.handle.info()) << " [" << join(ids) << "]";
  }
  const std::string def = default_backend();
  os << "; default: " << (def.empty() ? "none" : def);
  log("INFO", os.str());
}

std::vector<CameraEntry> CameraManager::list_cameras() const
{
  std::lock_guard<std::mutex> lock(mtx_);
  std::vector<CameraEntry> out;
  for (const auto & b : backends_.backends) {
    for (const auto & d : b.devices) {
      out.push_back({backend_name(b.handle.info()), d.id, d.description});
    }
  }
  return out;
}

std::string CameraManager::default_backend() const
{
  const std::string & first = backends_.first;
  if (first.empty()) {
    return "";
  }
  for (const auto & b : backends_.backends) {
    if (b.handle.info().name == first) {
      return backend_name(b.handle.info());
    }
  }
  if (const auto * info = registry_.find(first)) {
    return backend_name(*info);
  }
  return first;  // configured but not installed: `first` is the backend name
}

std::vector<std::string> CameraManager::loaded_backends() const
{
  std::lock_guard<std::mutex> lock(mtx_);
  std::vector<std::string> out;
  for (const auto & b : backends_.backends) {
    out.push_back(backend_name(b.handle.info()));
  }
  return out;
}

const CameraManager::Loaded * CameraManager::find_loaded(const std::string & backend,
    std::string & error) const
{
  // A skipped entry is named by its plugin name (or by the backend name when missing).
  auto why_skipped = [this](const std::string & wanted, bool by_plugin_name) {
    for (const auto & s : backends_.skipped) {
      const auto * info = registry_.find(s.name);
      const bool match =
          by_plugin_name ? s.name == wanted :
                           (s.name == wanted || (info != nullptr && backend_name(*info) == wanted));
      if (match) {
        return std::string(plugin_discovery::to_string(s.kind)) + ": " + s.reason;
      }
    }
    return std::string();
  };

  std::vector<std::string> loaded;
  for (const auto & b : backends_.backends) {
    loaded.push_back(backend_name(b.handle.info()));
  }
  const std::string loaded_text = loaded.empty() ? "none" : join(loaded);

  if (backend.empty()) {
    const std::string & first = backends_.first;
    if (first.empty()) {
      error = "no camera backend is configured or installed (loaded: none)";
      return nullptr;
    }
    for (const auto & b : backends_.backends) {
      if (b.handle.info().name == first) {
        return &b;
      }
    }
    std::string reason = why_skipped(first, true);
    error = "the default camera backend '" + default_backend() + "' is not available (" +
            (reason.empty() ? "not loaded" : reason) + "); loaded: " + loaded_text +
            ". Name a backend explicitly to use another one";
    return nullptr;
  }

  for (const auto & b : backends_.backends) {
    if (backend_name(b.handle.info()) == backend || b.handle.info().name == backend) {
      return &b;
    }
  }
  std::string reason = why_skipped(backend, false);
  error = "camera backend '" + backend + "' is not available (" +
          (reason.empty() ? "not loaded" : reason) + "); loaded: " + loaded_text;
  return nullptr;
}

CameraManager::Slot * CameraManager::slot(int index, const char * what)
{
  return const_cast<Slot *>(static_cast<const CameraManager *>(this)->slot(index, what));
}

const CameraManager::Slot * CameraManager::slot(int index, const char * what) const
{
  if (index < 0 || static_cast<size_t>(index) >= slots_.size() ||
      slots_[index]->device == nullptr) {
    log("WARNING", std::string(what) + ": no camera with index " + std::to_string(index) +
                       "; create the camera first");
    return nullptr;
  }
  return slots_[index].get();
}

int CameraManager::create_camera(uint32_t camera_id, const std::string & backend)
{
  std::lock_guard<std::mutex> lock(mtx_);

  std::string error;
  const Loaded * loaded = find_loaded(backend, error);
  if (loaded == nullptr) {
    log("ERROR", "create_camera(" + std::to_string(camera_id) + "): " + error);
    return -1;
  }
  const auto & info = loaded->handle.info();
  const std::string name = backend_name(info);
  const std::string id = std::to_string(camera_id);

  plugin_discovery::DeviceInfo device_info{id, "", ""};
  bool reported = false;
  std::vector<std::string> ids;
  for (const auto & d : loaded->devices) {
    ids.push_back(d.id);
    if (d.id == id) {
      device_info = d;
      reported = true;
    }
  }
  if (!reported && !has_capability(info, kCapabilityUnlistedIds)) {
    log("ERROR", "create_camera(" + id + "): backend '" + name + "' does not report camera " + id +
                     " (it reports: " + (ids.empty() ? "none" : join(ids)) + ")");
    return -1;
  }

  for (const auto & s : slots_) {
    if (s->device != nullptr && s->backend == name && s->id == camera_id) {
      log("ERROR", "create_camera(" + id + "): camera " + id + " of backend '" + name +
                       "' already exists; destroy it first");
      return -1;
    }
  }

  std::unique_ptr<ICameraDevice> device;
  try {
    device = loaded->handle->open(device_info);
  } catch (const std::exception & e) {
    log("ERROR",
        "create_camera(" + id + "): backend '" + name + "' failed to open it: " + e.what());
    return -1;
  }
  if (device == nullptr) {
    log("ERROR", "create_camera(" + id + "): backend '" + name + "' failed to open it");
    return -1;
  }

  auto s = std::make_unique<Slot>();
  s->backend = name;
  s->id = camera_id;
  s->device = std::move(device);
  slots_.push_back(std::move(s));
  log("INFO", "created camera " + id + " on backend '" + name + "'");
  return static_cast<int>(slots_.size() - 1);
}

bool CameraManager::set_camera_parameter(int index, CameraConfigure & param)
{
  std::lock_guard<std::mutex> lock(mtx_);
  Slot * s = slot(index, "set_camera_parameter");
  if (s == nullptr) {
    return false;
  }
  s->param = std::make_shared<CameraConfigure>(param);
  return true;
}

bool CameraManager::register_callback(int index, ImageCallback image_cb)
{
  std::lock_guard<std::mutex> lock(mtx_);
  Slot * s = slot(index, "register_callback");
  if (s == nullptr) {
    return false;
  }
  if (!image_cb) {
    log("WARNING", "register_callback: empty callback");
    return false;
  }
  auto listener = std::make_unique<FrameListener>(std::move(image_cb));
  s->device->add_listener(listener.get());
  s->listeners.push_back(std::move(listener));
  return true;
}

bool CameraManager::start_camera(int index)
{
  std::lock_guard<std::mutex> lock(mtx_);
  Slot * s = slot(index, "start_camera");
  if (s == nullptr) {
    return false;
  }
  if (s->param == nullptr) {
    log("WARNING", "start_camera: set the camera parameters before starting the camera");
    return false;
  }
  if (s->device->working()) {
    log("WARNING", "start_camera: camera " + std::to_string(s->id) + " is already started");
    return false;
  }

  std::vector<CameraParam> param;
  convert_param(*s->param, param);
  s->device->set_param(param);

  if (!s->device->init()) {
    log("ERROR", "start_camera: init of camera " + std::to_string(s->id) + " on backend '" +
                     s->backend + "' failed");
    return false;
  }
  const int ret = s->device->start_camera();
  if (ret != 0) {
    log("ERROR", "start_camera: camera " + std::to_string(s->id) + " on backend '" + s->backend +
                     "' failed to start (" + std::to_string(ret) + ")");
    return false;
  }
  log("INFO", "started camera " + std::to_string(s->id) + " on backend '" + s->backend + "'");
  return true;
}

void CameraManager::stop_locked(Slot & s)
{
  if (s.device == nullptr) {
    return;
  }
  if (s.device->working()) {
    const int ret = s.device->stop_camera();
    if (ret != 0) {
      log("WARNING", "stop_camera: camera " + std::to_string(s.id) + " on backend '" + s.backend +
                         "' returned " + std::to_string(ret));
    }
  }
  // After stopping, so no frame is being delivered to a listener that is going away.
  for (auto & listener : s.listeners) {
    s.device->remove_listener(listener.get());
  }
  s.listeners.clear();
}

void CameraManager::stop_camera(int index)
{
  std::lock_guard<std::mutex> lock(mtx_);
  Slot * s = slot(index, "stop_camera");
  if (s != nullptr) {
    stop_locked(*s);
  }
}

void CameraManager::destroy_locked(Slot & s)
{
  stop_locked(s);
  s.device.reset();  // while the backend that created it is still loaded
  s.param.reset();
}

bool CameraManager::destroy_camera(int index)
{
  std::lock_guard<std::mutex> lock(mtx_);
  Slot * s = slot(index, "destroy_camera");
  if (s == nullptr) {
    return false;
  }
  destroy_locked(*s);
  return true;
}

std::string CameraManager::camera_backend(int index) const
{
  std::lock_guard<std::mutex> lock(mtx_);
  const Slot * s = slot(index, "camera_backend");
  return s == nullptr ? "" : s->backend;
}

int64_t CameraManager::device_now_ns(int index) const
{
  std::lock_guard<std::mutex> lock(mtx_);
  const Slot * s = slot(index, "device_now_ns");
  return s == nullptr ? -1 : s->device->now_ns();
}
}  // namespace qrb_camera
