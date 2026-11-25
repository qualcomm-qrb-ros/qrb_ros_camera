# qrb_ros_camera cross-platform design

One `qrb_ros_camera` works on every machine whose camera stack has a backend; today there are two, QMMF/CamX and QCarCam. The vendor SDK is neither hard-coded nor chosen by detecting the SoC: [plugin_discovery](https://github.com/qualcomm-qrb-ros/plugin_discovery) (≥ 0.3.0) loads the backends listed by the platform configuration, and each backend's `probe()` reports whether it works on this machine and which cameras it can serve.

Related documents: [README.md](README.md) (build, deployment, running, known issues); plugin_discovery's `docs/CROSS_PLATFORM_DESIGN.md` (top-level design, principles D1–D9) and `docs/LIBRARY.md` (API, configuration rules).

---

## 1. Goals and scope

| Goal | How |
|---|---|
| One code base and one Ubuntu image for every machine | One backend plugin per vendor SDK; the configuration decides which one is loaded |
| No SoC check in the code | The machine only selects the platform configuration (section 7); the code of this repository never reads it |
| ROS is only one of the users | The `qrb_camera` libraries link no ROS library at run time (they are built with ament_cmake, like the original qrb_camera, so that colcon, bloom and meta-ros build them as usual); `qrb_ros_camera` is one of their users |
| Extensible by customers | A customer writes a backend and adds a customer configuration file; this repository is not changed |
| Ubuntu (LU) and Qualcomm Linux (LE) | Same code; only the packaging and the installation of the platform configuration differ (section 7) |

Out of scope: plugin_discovery's `compat` fallback (parsed but not used by the library yet), snapshot, a V4L2 backend, and the known issues of section 10.

## 2. Structure

```
plugin_discovery (external dependency)
   ▲
qrb_camera/                          one ament_cmake package; the libraries do not depend on ROS at run time
├── camera_interface   (target)     headers only: domain interface camera 1.0
├── qrb_camera         (target)     CameraManager: loads the backends through plugin_discovery;
│                                    links no backend and no vendor SDK
├── camera_backend_qmmf    (plugin)  links the QMMF SDK
└── camera_backend_qcarcam (plugin)  links qcxclient / qcxosal
   ▲
qrb_ros_camera/                      ROS 2 component node, depends on qrb_camera only
```

| Target | Installs | Exported as |
|---|---|---|
| `camera_interface` | `include/camera_interface/` | `qrb_camera::camera_interface`: what a backend (ours or a customer's) links |
| `qrb_camera` | `lib/libqrb_camera.so.3`, `include/qrb_camera/` | `qrb_camera::qrb_camera`: what users link |
| `camera_backend_qmmf` | `lib/camera/libcamera_backend_qmmf.so`, `share/camera/plugins/camera_backend_qmmf.plugin.yaml` | Not exported: a plugin, found through its manifest |
| `camera_backend_qcarcam` | `lib/camera/libcamera_backend_qcarcam.so`, `share/camera/plugins/camera_backend_qcarcam.plugin.yaml` | Same |

The platform configuration is not in this repository: it comes from the `platform-config` branch of plugin_discovery (section 7).

## 3. Interface `camera_interface` (camera 1.0)

```cpp
namespace qrb_camera {
class ICameraDevice {
public:
  explicit ICameraDevice(uint32_t id);
  virtual void add_listener(ICameraListener * listener) = 0;
  virtual void remove_listener(ICameraListener * listener) = 0;
  virtual void set_param(std::vector<CameraParam> & param) = 0;
  virtual bool init() = 0;
  virtual int  start_camera() = 0;
  virtual int  stop_camera() = 0;
  // Current time (ns) of the clock of the frame timestamps. QMMF: CLOCK_BOOTTIME; QCarCam: QTimer.
  virtual int64_t now_ns() const = 0;
  bool working() const;
  uint32_t get_id() const;
};

class ICameraBackend : public plugin_discovery::Backend {   // probe() comes from Backend
public:
  virtual std::string name() const = 0;   // "qmmf" / "qcarcam", the backend: of the manifest
  virtual std::unique_ptr<ICameraDevice> open(const plugin_discovery::DeviceInfo & device) = 0;
};
}
PLUGIN_DISCOVERY_INTERFACE(qrb_camera::ICameraBackend, "camera", 1, 0)
```

- `camera_types.hpp`: `StreamType`, `CameraFrame`, `ImageCallback`, `StreamParam`, `CameraParam`, `CameraConfigure`, intrinsics and extrinsics, i.e. the original `qrb_camera_common.hpp` plus `ImageCallback`. `camera_interface.hpp`: `ICameraFrame`, `ICameraListener`, `ICameraDevice`, `ICameraBackend`.
- `DeviceInfo.id` is the camera id as a decimal string: QMMF camera id or QCarCam inputId.
- A backend that cannot enumerate its cameras declares the capability `unlisted_ids`; any camera id may then be passed to `open()`.
- Versioning: bump the minor version only when appending virtual functions at the end, the major version otherwise. plugin_discovery checks at load time: same major, backend minor not below the user's minor, same ABI tag.

## 4. Backends

### 4.1 Registration and build

| | qmmf | qcarcam |
|---|---|---|
| Plugin name / class | `camera/QMMFCamera` / `qrb_camera::QMMFBackend` | `camera/QCarCamCamera` / `qrb_camera::QCarCamBackend` |
| `backend` (name used in configurations) | `qmmf` | `qcarcam` |
| origin / capabilities | `qcom` / `unlisted_ids` | `qcom` / — |
| SDK detection | `qmmf-sdk/qmmf_recorder.h`; `camera_metadata`, `qmmf_recorder_client`, `qmmf_camera_metadata`, `qmmf_utils` | `qcarcam.h`; `qcxclient`, `qcxosal` |
| CMake switch | `QRB_CAMERA_BACKEND_QMMF` | `QRB_CAMERA_BACKEND_QCARCAM` |

Each switch takes one of three values:
- `AUTO` (default): built when its SDK is found, otherwise skipped with a WARNING;
- `ON`: required; a missing SDK fails the configuration;
- `OFF`: not built.

A package that ships one backend (a Debian binary package, a Yocto `PACKAGECONFIG`) uses `ON`, so that a missing SDK cannot silently produce an empty package. A header that `find_path` does not find but the compiler does (the way the original IQ9/IQ10 builds worked) counts as found.

The `backend` name is an **external contract**: platform configurations refer to it, so renaming it means changing every platform configuration.

**What is decided when** (no stage needs a backend named by hand):

| Stage | Decides | Decided by |
|---|---|---|
| Build | Which backends are built | Which vendor SDKs are present (`AUTO`), or the packager (`ON`/`OFF`) |
| Image | Which backends are installed on the board | LU: the unified image installs all of them. LE: each machine's image installs its own backend only |
| Run time | Which backend is used | The platform configuration (first entry = default) plus `probe()`. The `camera_backend` parameter and `PLUGIN_DISCOVERY_BACKEND_CAMERA` only override it by hand |

### 4.2 `probe()`

Contract: `probe()` is called once per backend object; it only probes, it does not open or start a camera; on Unsupported or Fault it does not crash and leaves nothing behind. On Ok the backend stays loaded and may hold process-wide SDK state.

| | qmmf | qcarcam |
|---|---|---|
| Unsupported | `Connect` fails (no QMMF service) | — (only loaded where a configuration lists it, so every failure is a Fault) |
| Fault | — | `QCarCamInitialize` fails (QCX server not running), or `QCarCamQueryInputs` is empty; Uninitialize before returning |
| Ok | Reports camera `"0"` and declares `unlisted_ids`: like the original IQ9 code it does not enumerate cameras; `camera_id` comes from the caller and QMMF checks it when the camera is opened | One device per inputId; the SDK stays initialized |
| SDK state after probe | A temporary `Connect`, then `Disconnect`; each device's Recorder later connects on its own | Held by the backend (4.3) |

### 4.3 The QCarCam SDK is initialized by the backend

```
QCarCamBackend::probe()   QCarCamInitialize → QCarCamQueryInputs → stays initialized on success
QCarCamBackend::open(id)  new QCarCamCamera(id): the camera never calls Initialize/Uninitialize; it only does
                          Open / SetParam / SetBuffers / Reserve / Start / Stop / Release / Close
~QCarCamBackend()         QCarCamUninitialize
```

The original IQ10 code initialized the SDK in each camera's `init()` and released it in `stop_camera()`, so with two cameras in one node, stopping the first one tore down the session of the second. Now all cameras of one `CameraManager` share one backend, which initializes the SDK once. Section 5 guarantees the destruction order: every camera is destroyed before its backend. The remaining limitation is several nodes in one process (section 10).

### 4.4 Differences from the original IQ9/IQ10 code

The vendor SDK call sequences, the buffer allocation and the buffer return mechanisms are unchanged. Changes:

| Area | Original | Now |
|---|---|---|
| Backend selection | `CameraType::QMMF` / `QCarCam` fixed at build time, one repository per platform | plugin_discovery configuration plus `probe()`; neither the node nor the manager knows a backend by name |
| Listener | `QMMFListener` and `QCarCamListener`, two copies of the same logic | One generic `FrameListener` in `qrb_camera`, not in a backend library |
| Clock | `CLOCK_BOOTTIME` (IQ9) or QTimer (IQ10) fixed in the node | Provided by the backend through `now_ns()`; the QTimer code moved into the qcarcam backend |
| QCarCam SDK initialization | Once per camera | Once per backend (4.3); the commented-out `wait_for_qcx` was removed |
| Return values of `init()` / `start_camera()` | Not checked on IQ10 | Checked: a failure returns false and the node throws |

QMMF issues fixed while porting (marked `Fix:` in the sources):

| Original issue | Change |
|---|---|
| `QMMFFrame::release_ref()` deleted the frame while holding its own mutex, then decremented the freed `refs_` | Compute the remaining count under the lock, delete after releasing it |
| With no listener or an empty buffer, the frame allocated first was leaked | Check first, allocate afterwards |
| `stop_camera()` erased elements of a container while iterating over it (iterator invalidation) | Clear after the loop; on an error, keep tearing down the remaining resources and return -1 at the end |
| `init()`, called before every start, appended the streams again, so a second start duplicated the tracks | `init()` clears and rebuilds them; it does not `Connect` again when already connected |
| An unknown format string left `VideoFormat` uninitialized | Log a WARNING and use nv12 |
| After `init()` or `start` failed half-way, the Recorder stayed connected and the camera started | The destructor calls `StopCamera` and `Disconnect` as needed |

## 5. `qrb_camera::CameraManager`

```cpp
class CameraManager {
public:
  explicit CameraManager(std::vector<std::string> manual_backends = {},
                         std::vector<std::string> plugin_paths = {});
  std::vector<CameraEntry> list_cameras() const;
  std::string default_backend() const;
  std::vector<std::string> loaded_backends() const;
  int  create_camera(uint32_t camera_id, const std::string & backend = "");
  bool set_camera_parameter(int index, CameraConfigure & param);
  bool register_callback(int index, ImageCallback image_cb);
  bool start_camera(int index);            // set_param → init → start_camera
  void stop_camera(int index);
  bool destroy_camera(int index);
  std::string camera_backend(int index) const;
  int64_t device_now_ns(int index) const;
};
```

**Where the manifests are searched** (when `plugin_paths` is empty):
- `$PLUGIN_DISCOVERY_PATH` if set;
- otherwise, in this order: the built-in `<prefix>/share/camera/plugins`; `share/camera/plugins` under every prefix of `$AMENT_PREFIX_PATH` and `$CMAKE_PREFIX_PATH`; `share/camera/plugins` and `*/share/camera/plugins` under `$COLCON_PREFIX_PATH`.

The last one is for colcon, which by default installs every package into its own prefix: the packages of this repository are put on `AMENT_PREFIX_PATH`, but a customer backend written as a plain CMake package may be on no path variable at all.

**`create_camera(camera_id, backend)`**:
1. `backend` given: look it up by name among the loaded backends.
2. `backend` empty: use the first entry of the merged configuration (`BackendSet::first`). If it is not loaded, fail and log why (missing, unsupported, fault, incompatible, load_failed); **no fallback** to another backend.
3. `camera_id` must be one of the devices the backend reported, unless it declares `unlisted_ids`.
4. The same backend and id cannot be created twice.

**Lifetime**: the members are declared in the order registry, `BackendSet`, cameras. On destruction the cameras go first, then the backend objects, and only then are their libraries unloaded.

## 6. ROS node `qrb_ros_camera`

| Parameter | Default | Description |
|---|---|---|
| `camera_backend` | `""` | Empty: decided by the configuration. Otherwise passed to `CameraManager` as a manual choice: only this backend is loaded |
| `camera_id` | required | The backend's camera id (QMMF camera id or QCarCam inputId) |
| `stream_size`, `stream_name`, `<stream>.width/height/fps` | as in the original | |
| `camera_info_path` | in the launch file: `share/qrb_ros_camera/config/camera_info_OX03F10_yuv.yaml` | Only fills the intrinsics of the `camera_info` topic; does not affect the frames |

**Construction order**: parse the parameters → `CameraManager(manual)` → `create_camera` → compute the time offset and start a 1 s timer → `set_camera_parameter` → `register_callback` → `start_camera`. Any failure throws, so `component_container` fails to load the node instead of keeping a node that never publishes.

**Time offset**: `offset = ROS time − device_now_ns()`, and on publish `stamp = frame.timestamp + offset` (clamped to 0 on underflow). The offset is **recomputed every second**, because:
- ROS time is the system clock, which time synchronization (NTP, gPTP) slews and may step; the QTimer is a hardware counter that is never adjusted.
- Measured with the qcarcam backend on IQ10, the two drifted apart by up to about 33 ppm. Computed once at start, the offset would be off by about 120 ms per hour; recomputed every second, the error stays below about 33 us.
- A step of the system clock affects at most 1 s of frames; a change of the offset by more than 1 ms is logged as a WARN.
- It is not recomputed per frame, to keep the camera callback threads away from the `CameraManager` lock.
- With the qmmf backend, `CLOCK_BOOTTIME` is slewed together with the system clock, so it does not drift, but it is affected by steps all the same.

## 7. Configuration and deployment

The platform configuration says which backends to load on a machine. It lives on the **`platform-config` branch** of plugin_discovery, as `camera/<machine>-<os>-camera.config.yaml`:

| Files | Content |
|---|---|
| `QCS9075-Ubuntu-camera.config.yaml`, `QCS9075-QCLinux-camera.config.yaml` (IQ9) | `backends: [qmmf]` |
| `QAM8797P-Ubuntu-camera.config.yaml`, `QAM8797P-QCLinux-camera.config.yaml` (IQ10) | `backends: [qcarcam]` |

`<machine>` is the content of `/sys/devices/soc0/machine`. A configuration file holds backend names only; camera ids, resolutions and calibration files are parameters of the node.

| | LU (Ubuntu) | LE (Qualcomm Linux) |
|---|---|---|
| Installing the configuration | The deb `plugin-discovery-platform-config` installs every candidate into `/usr/share/plugin_discovery/platforms.d/`; `platform-detect` (run at the first boot, and by the deb's postinst) links the one matching this machine and OS to `/etc/plugin_discovery/platform.d/camera.config.yaml` | The root file system is read-only and an image is built for one machine: the recipe `plugin-discovery-platform-config` installs the file of `PD_MACHINE` directly as `/etc/plugin_discovery/platform.d/camera.config.yaml` at build time; `platform-detect` is not used |
| Installing the backends | The unified image installs all backends; at run time the configuration decides which one is loaded, and a backend not in the list is never dlopen'd | The image installs this machine's backend only (the recipe's `PACKAGECONFIG` and split packages) |

This repository needs nothing special for either system: `CameraManager` only reads the configuration, writes no file and does not depend on `platform-detect`; the default manifest directory comes from `CMAKE_INSTALL_FULL_DATADIR`, i.e. `/usr/share/camera/plugins` with `prefix=/usr`.

**When the machine does not match**: on LU the machine string must match the candidate file name (case is ignored). Without a match the domain has no configuration, the library loads every compatible backend, and the default, ordered by plugin name, is qcarcam, so a machine without QCarCam fails.

**Customer configuration**: `NN-camera-<source>.config.yaml` in `/etc/plugin_discovery/config.d/` applies after the platform configuration (`backends`, `disable`, `reset`; rules in plugin_discovery's `docs/TEMPLATES.md`).

**Contract with plugin_discovery** (if any of these changes, this repository has to follow): the configuration format; naming backends by hand (`load(manual)`, `PLUGIN_DISCOVERY_BACKEND_CAMERA`; the node's `camera_backend` parameter relies on it); overriding the configuration directories at run time (`PLUGIN_DISCOVERY_CONFIG_PATH`; the unit test relies on it).

## 8. Run-time flow (example: a machine configured with qcarcam)

```
configuration: /etc/plugin_discovery/platform.d/camera.config.yaml = backends: [qcarcam]
ros2 launch qrb_ros_camera qrb_ros_camera_launch.py camera_id:=5
  CameraNode
    → CameraManager(): scan() reads manifests and configuration → list [qcarcam], first = camera/QCarCamCamera
    → load<ICameraBackend>(): dlopen the qcarcam library only → version/ABI check → probe() → Ok {5}
    → create_camera(5, "") → open({5}) → set_param → init → start_camera
    → frames → FrameListener → publish (timestamps converted to ROS time with device_now_ns())
```

## 9. Testing

- **Unit test** (`qrb_camera/test/camera_manager_test`; needs no vendor SDK and no camera): four fake backend plugins test the default backend, manual choice (parameter and environment), failure without fallback when the default is missing/unsupported/fault/throws in probe/ABI-incompatible, no configuration, `unlisted_ids`, camera id checks, `open`/`init`/`start` failures, frames and timestamps, destruction order, and finding manifests in colcon install trees.
- **On device**: verified on IQ9 and IQ10 (Ubuntu): build (a backend without its SDK is skipped), selection by the platform configuration, frames (30 fps), positive and stable latency, timestamps consistent with the system clock, restart after a clean stop, and the selection rules and failure paths: manual choice, naming a backend the machine does not have, unknown camera id, no configuration, a customer default that is not installed, customer configuration plus manual choice.
- LE has not been verified on a device yet.

## 10. Known issues and limitations

| # | Issue | Status |
|---|---|---|
| 1 | **Frame data is invalid once the callback returns (both backends)**: the node publishes the dma-buf fd of the backend's buffer; when the callback returns, the buffer goes back to the driver or the recorder and is overwritten a few frames later, so a slow subscriber reads a later frame. The original code behaves the same | Options: dup the fd per message; an optional copy mode; return the buffer only when the subscribers are done (a real reference count for QCarCam too) |
| 2 | **Several camera nodes in one process initialize QCarCam several times**: each node has its own backend, and the first one unloaded calls `QCarCamUninitialize`, which invalidates the other's session. The original code had the same problem with several cameras in one node | When needed: a process-wide reference count inside the backend library |
| 3 | **A fatal error at run time is not reported to the node**: QCarCam's `QCARCAM_ERROR_FATAL` and QMMF's `kCameraError` are only logged; the node stays alive but publishes nothing. Same in the original code | Option: a frame watchdog in the node that rebuilds `CameraManager` on timeout, with back-off |
| 4 | After the QCX server restarts at run time, the session held by the backend is invalid | Restart the node (the watchdog of issue 3 would cover it) |
| 5 | If the node starts before QCX is ready, probe reports Fault | `qcx_server` starts at boot and the node is started afterwards, so this does not happen in practice; a node started at boot needs `After=` and `Wants=qcx_server.service` in its unit |
| 6 | With several QCarCam streams, only the first one receives frames: every buffer list id is the constant 5, probably the output port number of the ISP usecase | Needs the QCarCam documentation of the output port ids; one stream is configured today |
| 7 | When QCarCam stops, the callback thread walks `streams_` without a lock | Repeated start/stop works (`QCarCamStop` probably waits for the callbacks), but it is not documented |
| 8 | The frame rate parameter has little effect: QMMF's `StartCamera` is fixed at 30 fps; QCarCam only logs the fps | — |
| 9 | The format is fixed to nv12; no snapshot | The parameter parser has no format parameter and creates Track streams only |
| 10 | On Ubuntu images, `apt install` lets needrestart restart platform services; `qcx_server.service` runs `restorecon` in `ExecStartPre`, which some images lack, so QCX does not come back after a restart | Install local debs with `dpkg -i`; use `NEEDRESTART_MODE=l` with apt; report the image issue to the platform owner |
| 11 | plugin_discovery is still 0.x; behaviour marked "provisional" in its documentation may change | `find_package(plugin_discovery 0.3)` pins the version |
