/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

// CameraManager on fake backends (no vendor SDK, no ROS): backend selection by
// configuration, manual override, failure without fallback, camera ids, the camera
// lifecycle, and destruction order.
//
// Each case writes manifests (pointing at the fake backend libraries) and a configuration
// into a temporary directory. Run: camera_manager_test   (exit code 0 = all passed)

#include "qrb_camera/camera_manager.hpp"

#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "fake_events.hpp"

#ifndef FAKE_BACKEND_DIR
#error "FAKE_BACKEND_DIR must point at the directory of the fake backend libraries"
#endif

namespace fs = std::filesystem;
using qrb_camera::CameraManager;

namespace
{
int g_failures = 0;
int g_checks = 0;
std::string g_case;

#define CHECK(cond) check((cond), #cond, __LINE__)

void check(bool ok, const char * text, int line)
{
  ++g_checks;
  if (!ok) {
    ++g_failures;
    std::cerr << "FAIL  [" << g_case << "] line " << line << ": " << text << std::endl;
  }
}

struct FakePlugin
{
  const char * cls;
  const char * backend;
  const char * capabilities;
};

const FakePlugin kPlugins[] = {
    {"FakeA", "fake_a", ""},
    {"FakeB", "fake_b", ""},
    {"FakeAny", "fake_any", "unlisted_ids"},
    {"FakeAbi", "fake_abi", ""},
};

// A scratch directory with plugins/ (manifests) and config/ (platform tier).
class Sandbox
{
public:
  Sandbox()
  {
    root_ = fs::temp_directory_path() /
            ("qrb_camera_test_" + std::to_string(::getpid()) + "_" + std::to_string(counter_++));
    fs::remove_all(root_);
    fs::create_directories(root_ / "config");
    write_manifests(root_ / "plugins", kPlugins, std::size(kPlugins));
    ::setenv("PLUGIN_DISCOVERY_CONFIG_PATH", (root_ / "config").c_str(), 1);
  }

  // Manifests of `count` fake plugins into `dir`.
  static void write_manifests(const fs::path & dir, const FakePlugin * plugins, size_t count)
  {
    fs::create_directories(dir);
    for (size_t i = 0; i < count; ++i) {
      const auto & p = plugins[i];
      std::ofstream(dir / (std::string(p.cls) + ".plugin.yaml"))
          << "plugins:\n"
             "  - name: camera/"
          << p.cls
          << "\n"
             "    class: fake_camera::"
          << p.cls
          << "\n"
             "    base: qrb_camera::ICameraBackend\n"
             "    library: "
          << (fs::path(FAKE_BACKEND_DIR) / ("libfake_backend_" + std::string(p.cls) + ".so"))
                 .string()
          << "\n"
             "    domain: camera\n"
             "    backend: "
          << p.backend
          << "\n"
             "    origin: qcom\n"
             "    capabilities: ["
          << p.capabilities << "]\n";
    }
  }

  const fs::path & root() const { return root_; }

  ~Sandbox()
  {
    std::error_code ec;
    fs::remove_all(root_, ec);
  }

  // The platform configuration: camera backends in this order.
  void configure(const std::string & backends)
  {
    std::ofstream(root_ / "config" / "camera.config.yaml")
        << "domains:\n  camera:\n    backends: [" << backends << "]\n";
  }

  std::vector<std::string> plugin_paths() const { return {(root_ / "plugins").string()}; }

private:
  fs::path root_;
  static inline int counter_ = 0;
};

void clear_env()
{
  for (const char * name : {"FAKE_A", "FAKE_B", "FAKE_ANY", "FAKE_ABI"}) {
    for (const char * knob : {"PROBE", "DEVICES", "OPEN", "INIT", "START"}) {
      ::unsetenv((std::string("FAKE_CAMERA_") + name + "_" + knob).c_str());
    }
  }
  ::unsetenv("PLUGIN_DISCOVERY_BACKEND_CAMERA");
  ::unsetenv("PLUGIN_DISCOVERY_PATH");
  fake_events::take();
}

void set_env(const std::string & name, const std::string & value)
{
  ::setenv(name.c_str(), value.c_str(), 1);
}

qrb_camera::CameraConfigure one_stream(uint32_t width = 1280, uint32_t height = 720)
{
  qrb_camera::CameraConfigure c;
  c.stream_size = 1;
  c.stream_names = {"stream1"};
  c.stream_formats = {"nv12"};
  c.stream_widths = {width};
  c.stream_heights = {height};
  c.stream_fps = {30};
  c.stream_type = {qrb_camera::StreamType::Track};
  return c;
}

size_t index_of(const std::vector<std::string> & events, const std::string & event)
{
  return static_cast<size_t>(std::find(events.begin(), events.end(), event) - events.begin());
}

bool contains(const std::vector<std::string> & events, const std::string & event)
{
  return index_of(events, event) < events.size();
}

// ---------------------------------------------------------------------------------------

void default_is_first_configured()
{
  Sandbox box;
  box.configure("fake_b, fake_a");
  CameraManager m({}, box.plugin_paths());
  CHECK(m.default_backend() == "fake_b");
  CHECK((m.loaded_backends() == std::vector<std::string>{"fake_b", "fake_a"}));

  const int b5 = m.create_camera(5);
  CHECK(b5 >= 0);
  CHECK(m.camera_backend(b5) == "fake_b");
  CHECK(m.create_camera(0) == -1);  // the default does not report 0: no fallback
  const int a0 = m.create_camera(0, "fake_a");
  CHECK(a0 >= 0);
  CHECK(m.camera_backend(a0) == "fake_a");
  CHECK(m.create_camera(0, "fake_a") == -1);        // already exists
  CHECK(m.create_camera(7, "fake_a") == -1);        // not reported
  CHECK(m.create_camera(0, "camera/FakeA") == -1);  // plugin name works too, but exists

  const auto cams = m.list_cameras();
  CHECK(cams.size() == 3);  // fake_b: 5, fake_a: 0, 1
  CHECK(!cams.empty() && cams[0].backend == "fake_b" && cams[0].id == "5");
}

void config_lists_only_some()
{
  Sandbox box;
  box.configure("fake_a");
  CameraManager m({}, box.plugin_paths());
  CHECK((m.loaded_backends() == std::vector<std::string>{"fake_a"}));
  CHECK(m.create_camera(5, "fake_b") == -1);  // installed, not listed: not loaded
  const auto events = fake_events::take();
  CHECK(!contains(events, "fake_b backend ctor"));  // never even created
}

void default_unsupported_fails()
{
  Sandbox box;
  box.configure("fake_a, fake_b");
  set_env("FAKE_CAMERA_FAKE_A_PROBE", "unsupported");
  CameraManager m({}, box.plugin_paths());
  CHECK(m.default_backend() == "fake_a");
  CHECK((m.loaded_backends() == std::vector<std::string>{"fake_b"}));
  CHECK(m.create_camera(5) == -1);           // the default is not loaded: no fallback
  CHECK(m.create_camera(5, "fake_b") >= 0);  // naming it works
}

void default_fault_or_throw_fails()
{
  for (const char * mode : {"fault", "throw"}) {
    clear_env();
    Sandbox box;
    box.configure("fake_a, fake_b");
    set_env("FAKE_CAMERA_FAKE_A_PROBE", mode);
    CameraManager m({}, box.plugin_paths());
    CHECK(m.create_camera(0) == -1);
    CHECK(m.create_camera(5, "fake_b") >= 0);
  }
}

void default_missing_fails()
{
  Sandbox box;
  box.configure("ghost, fake_a");
  CameraManager m({}, box.plugin_paths());
  CHECK(m.default_backend() == "ghost");
  CHECK(m.create_camera(0) == -1);
  CHECK(m.create_camera(0, "fake_a") >= 0);
}

void default_incompatible_fails()
{
  Sandbox box;
  box.configure("fake_abi, fake_a");
  CameraManager m({}, box.plugin_paths());
  CHECK(m.default_backend() == "fake_abi");
  CHECK((m.loaded_backends() == std::vector<std::string>{"fake_a"}));
  CHECK(m.create_camera(0) == -1);
  const auto events = fake_events::take();
  CHECK(!contains(events, "fake_abi backend ctor"));  // rejected before any object exists
}

void manual_replaces_configuration()
{
  Sandbox box;
  box.configure("fake_b, fake_a");
  {
    CameraManager m({"fake_a"}, box.plugin_paths());
    CHECK((m.loaded_backends() == std::vector<std::string>{"fake_a"}));
    CHECK(m.default_backend() == "fake_a");
    CHECK(m.create_camera(0) >= 0);
  }
  set_env("PLUGIN_DISCOVERY_BACKEND_CAMERA", "fake_a");
  {
    CameraManager m({}, box.plugin_paths());
    CHECK((m.loaded_backends() == std::vector<std::string>{"fake_a"}));
    CHECK(m.default_backend() == "fake_a");
  }
}

void no_configuration_loads_all()
{
  Sandbox box;  // empty config directory
  CameraManager m({}, box.plugin_paths());
  // All compatible backends, by plugin name: camera/FakeA, camera/FakeAny, camera/FakeB.
  CHECK((m.loaded_backends() == std::vector<std::string>{"fake_a", "fake_any", "fake_b"}));
  CHECK(m.default_backend() == "fake_a");
}

void unlisted_ids()
{
  Sandbox box;
  box.configure("fake_any");
  CameraManager m({}, box.plugin_paths());
  CHECK(m.create_camera(42) >= 0);  // not reported, but the backend declares unlisted_ids
}

void empty_device_list()
{
  Sandbox box;
  box.configure("fake_a");
  set_env("FAKE_CAMERA_FAKE_A_DEVICES", "");
  CameraManager m({}, box.plugin_paths());
  CHECK(m.create_camera(0) == -1);
}

void open_failures()
{
  for (const char * mode : {"null", "throw"}) {
    clear_env();
    Sandbox box;
    box.configure("fake_a");
    set_env("FAKE_CAMERA_FAKE_A_OPEN", mode);
    CameraManager m({}, box.plugin_paths());
    CHECK(m.create_camera(0) == -1);
  }
}

void lifecycle_and_frames()
{
  Sandbox box;
  box.configure("fake_a");
  std::vector<std::unique_ptr<qrb_camera::CameraFrame>> frames;
  {
    CameraManager m({}, box.plugin_paths());
    const int i = m.create_camera(1);
    CHECK(i >= 0);
    CHECK(!m.start_camera(i));  // no parameters yet
    auto cfg = one_stream(640, 480);
    CHECK(m.set_camera_parameter(i, cfg));
    CHECK(m.register_callback(i,
        [&frames](std::unique_ptr<qrb_camera::CameraFrame> f) { frames.push_back(std::move(f)); }));
    CHECK(m.start_camera(i));
    CHECK(!m.start_camera(i));  // already started
    CHECK(m.device_now_ns(i) == 1000001);

    CHECK(frames.size() == 1);
    if (!frames.empty()) {
      const auto & f = *frames[0];
      CHECK(f.stream_name == "stream1");
      CHECK(f.width == 640 && f.height == 480);
      CHECK(f.format == "nv12");
      CHECK(f.timestamp == 1000001 - 100);
      CHECK(f.frame_id == 1);
    }

    m.stop_camera(i);
    CHECK(m.destroy_camera(i));
    CHECK(!m.destroy_camera(i));
    CHECK(m.device_now_ns(i) == -1);
    CHECK(m.create_camera(1) >= 0);  // can be created again after destroy
  }
  const auto events = fake_events::take();
  CHECK(contains(events, "fake_a init 1"));
  CHECK(contains(events, "fake_a start 1"));
  CHECK(contains(events, "fake_a stop 1"));
}

void init_and_start_failures()
{
  for (const char * knob : {"FAKE_CAMERA_FAKE_A_INIT", "FAKE_CAMERA_FAKE_A_START"}) {
    clear_env();
    Sandbox box;
    box.configure("fake_a");
    set_env(knob, "fail");
    CameraManager m({}, box.plugin_paths());
    const int i = m.create_camera(0);
    auto cfg = one_stream();
    CHECK(m.set_camera_parameter(i, cfg));
    CHECK(!m.start_camera(i));
  }
}

void devices_destroyed_before_backends()
{
  Sandbox box;
  box.configure("fake_a, fake_b");
  {
    CameraManager m({}, box.plugin_paths());
    const int a = m.create_camera(0);
    const int b = m.create_camera(5, "fake_b");
    auto cfg = one_stream();
    m.set_camera_parameter(a, cfg);
    m.register_callback(a, [](std::unique_ptr<qrb_camera::CameraFrame>) {});
    CHECK(m.start_camera(a));
    CHECK(b >= 0);
    fake_events::take();
  }  // the manager goes with two open cameras, one of them started
  const auto e = fake_events::take();
  CHECK(index_of(e, "fake_a stop 0") < index_of(e, "fake_a device dtor 0"));
  CHECK(index_of(e, "fake_a device dtor 0") < index_of(e, "fake_a backend dtor"));
  CHECK(index_of(e, "fake_b device dtor 5") < index_of(e, "fake_b backend dtor"));
  CHECK(contains(e, "fake_a backend dtor") && contains(e, "fake_b backend dtor"));
}

void invalid_indices()
{
  Sandbox box;
  box.configure("fake_a");
  CameraManager m({}, box.plugin_paths());
  auto cfg = one_stream();
  for (int i : {-1, 0, 3}) {
    CHECK(!m.set_camera_parameter(i, cfg));
    CHECK(!m.register_callback(i, [](std::unique_ptr<qrb_camera::CameraFrame>) {}));
    CHECK(!m.start_camera(i));
    m.stop_camera(i);
    CHECK(!m.destroy_camera(i));
    CHECK(m.camera_backend(i).empty());
    CHECK(m.device_now_ns(i) == -1);
  }
}

// colcon's default isolated install: one prefix per package, nothing on PLUGIN_DISCOVERY_PATH,
// AMENT_PREFIX_PATH or CMAKE_PREFIX_PATH; only COLCON_PREFIX_PATH points at the install base.
// (Seen on IQ10: the backend was reported "configured but not installed".)
void colcon_isolated_install()
{
  Sandbox box;
  box.configure("fake_b, fake_a");
  const fs::path base = box.root() / "install";
  Sandbox::write_manifests(base / "fake_pkg_a" / "share" / "camera" / "plugins", &kPlugins[0], 1);
  Sandbox::write_manifests(base / "fake_pkg_b" / "share" / "camera" / "plugins", &kPlugins[1], 1);
  fs::create_directories(base / "unrelated_pkg" / "share");  // no plugins: ignored

  ::unsetenv("AMENT_PREFIX_PATH");
  ::unsetenv("CMAKE_PREFIX_PATH");
  ::setenv("COLCON_PREFIX_PATH", base.c_str(), 1);
  {
    CameraManager m;  // no plugin_paths: the manager searches
    CHECK((m.loaded_backends() == std::vector<std::string>{"fake_b", "fake_a"}));
    CHECK(m.create_camera(5) >= 0);
  }
  // A merged install: <base>/share/camera/plugins.
  const fs::path merged = box.root() / "merged";
  Sandbox::write_manifests(merged / "share" / "camera" / "plugins", &kPlugins[0], 2);
  ::setenv("COLCON_PREFIX_PATH", merged.c_str(), 1);
  {
    CameraManager m;
    CHECK((m.loaded_backends() == std::vector<std::string>{"fake_b", "fake_a"}));
  }
  ::unsetenv("COLCON_PREFIX_PATH");
}

void nothing_installed()
{
  Sandbox box;
  box.configure("fake_a");
  CameraManager m({}, {(fs::temp_directory_path() / "qrb_camera_test_no_such_dir").string()});
  CHECK(m.loaded_backends().empty());
  CHECK(m.list_cameras().empty());
  CHECK(m.create_camera(0) == -1);
}

void run(const char * name, const std::function<void()> & test)
{
  g_case = name;
  clear_env();
  const int before = g_failures;
  test();
  std::cout << (g_failures == before ? "PASS  " : "FAIL  ") << name << std::endl;
}
}  // namespace

int main()
{
  run("default is the first configured backend", default_is_first_configured);
  run("only the configured backends load", config_lists_only_some);
  run("default unsupported: fails, no fallback", default_unsupported_fails);
  run("default fault / probe throws: fails", default_fault_or_throw_fails);
  run("default configured but not installed: fails", default_missing_fails);
  run("default incompatible (ABI): fails", default_incompatible_fails);
  run("manual backends replace the configuration", manual_replaces_configuration);
  run("no configuration: all compatible backends", no_configuration_loads_all);
  run("unlisted_ids: any camera id", unlisted_ids);
  run("backend reports no camera", empty_device_list);
  run("open returns null / throws", open_failures);
  run("camera lifecycle and frames", lifecycle_and_frames);
  run("init / start failure is reported", init_and_start_failures);
  run("devices are destroyed before their backends", devices_destroyed_before_backends);
  run("invalid indices", invalid_indices);
  run("colcon install: manifests found without PLUGIN_DISCOVERY_PATH", colcon_isolated_install);
  run("nothing installed", nothing_installed);

  std::cout << (g_failures == 0 ? "all passed" : "FAILED") << " (" << g_checks - g_failures << "/"
            << g_checks << " checks)" << std::endl;
  return g_failures == 0 ? 0 : 1;
}
