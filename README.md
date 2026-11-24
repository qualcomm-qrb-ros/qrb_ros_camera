## 👋 Overview

The `QRB ROS Camera` is a ROS package to publish the images from Qualcomm **GMSL** cameras. It provides:

- Camera backends as plugins: qmmf and qcarcam, loaded through [plugin_discovery](https://github.com/qualcomm-qrb-ros/plugin_discovery).
- Platform and customer config select backends; manual selection overrides config.
- Customers can add, reorder, disable, or replace backends.
- Concurrent multiple streams output support.
- Composable ROS node support.
- Zero-Copy transport powered by `QRB ROS Transport`.
- Only support outputs in NV12 format


<div align="center">
  <img src="./docs/assets/architecture.png" alt="architecture">
</div>

<br>

The `qrb_ros_camera` is a ROS 2 package. It creates an image publisher with qrb_ros_transport for zero-copy transport. It supports node composition, making it possible to improve performance using ROS intra-process communication.

The `qrb_camera` is a C++ library, it provides APIs to `qrb_ros_camera` for querying images from the lower layer `QMMF` or `QCarCam` library. It includes:
- The module `camera_interface`: the camera backend interface (`ICameraBackend`, `ICameraDevice`).
- The module `camera_manager`: loads the camera backends through [plugin_discovery](https://github.com/qualcomm-qrb-ros/plugin_discovery) and manages camera streams.
- The backend `camera_backend_qmmf` : calls QMMF APIs.
- The backend `camera_backend_qcarcam` : calls QCarCam APIs and delivers frames via DMA-heap buffers.

The code never checks the SoC or the device: which backend a device uses is decided by the plugin_discovery configuration, so one build serves every device whose camera stack has a backend. A backend is built only when its vendor SDK is found.

The `qrb_ros_transport` is a ROS 2 package, it supports zero-copy image transport with Linux DMA buffer and implements ROS type adaption, make it compatible with both intra- and inter-process communication.

## 🔎 Table of contents
  * [APIs](#-apis)
     * [`qrb_ros_camera` APIs](#-qrb_ros_camera-apis)
     * [`qrb_camera` APIs](#-qrb_camera-apis)
  * [Supported targets](#-supported-targets)
  * [Usage](#-usage)
     * [Starting the camera node](#start-the-camera-node)
     * [Select the camera backend](#select-the-camera-backend)
     * [Enable mutiple streams](#enable-multiple-streams)
     * [Enable zero copy transport](#enable-zero-copy-transport)
  * [Build from source](#-build-from-source)
  * [License](#-license)

## ⚓ APIs

### 🔹 `qrb_ros_camera` APIs

#### ROS interfaces

<table>
  <tr>
    <th>Interface</th>
    <th>Name</th>
    <th>Type</th>
    <td>Description</td>
  </tr>
  <tr>
    <td>Publisher</td>
    <td>/cam${camera_id}_${stream_name}</td>
    <td>qrb_ros::transport::type::Image</td>
    <td>output image (zero-copy DMA-buf)</td>
  </tr>
  <tr>
    <td></td>
    <td>/cam${camera_id}_${stream_name}_camera_info</td>
    <td>sensor_msgs/msg/CameraInfo</td>
    <td>camera information (per stream)</td>
  </tr>
</table>

#### ROS parameters

<table>
  <tr>
    <th>Name</th>
    <th>Type</th>
    <th>Description</td>
    <th>Default Value</td>
  </tr>
  <tr>
    <td>camera_backend</td>
    <td>string</td>
    <td>Camera backend, e.g. qmmf or qcarcam. Empty: the default backend of the plugin_discovery configuration</td>
    <td>""</td>
  </tr>
  <tr>
    <td>camera_id</td>
    <td>int64</td>
    <td>The backend's camera id, e.g. the QMMF camera id or the QCarCam inputId</td>
    <td>0</td>
  </tr>
  <tr>
    <td>stream_size</td>
    <td>uint64</td>
    <td>Count of camera streams</td>
    <td>1</td>
  </tr>
  <tr>
    <td>stream_name</td>
    <td>string[]</td>
    <td>camera stream names</td>
    <td>["stream1"]</td>
  </tr>
  <tr>
    <td>${stream_name}.width</td>
    <td>uint32</td>
    <td>image width</td>
    <td>1280</td>
  </tr>
  <tr>
    <td>${stream_name}.height</td>
    <td>uint32</td>
    <td>image height</td>
    <td>720</td>
  </tr>
  <tr>
    <td>${stream_name}.fps</td>
    <td>uint32</td>
    <td>output image frequency(Hz)</td>
    <td>30</td>
  </tr>
  <tr>
    <td>camera_info_path</td>
    <td>string</td>
    <td>Camera intrinsic parameters YAML file path</td>
    <td>config/camera_info_OX03F10_yuv.yaml</td>
  </tr>
  <tr>
    <td>dump</td>
    <td>bool</td>
    <td>Dump received frames to disk (debug only)</td>
    <td>false</td>
  </tr>
</table>

> [!Note]
> The parameter values should be set according to the actual camera hardware capabilities.
>
> The launch file also takes `enable_test_node` (default `false`): it loads TestNode, which prints the fps and latency of the stream. TestNode exists only in a build with tests (`BUILD_TESTING=ON`).

### 🔹 `qrb_camera` APIs

<table>
  <tr>
    <th>Function</th>
    <th>Parameters</th>
    <th>Description</th>
  </tr>
  <tr>
    <td>int create_camera(uint32_t camera_id, const std::string & backend = "")</td>
    <td>camera_id: the backend's camera id, backend: backend name (empty: the default backend)</td>
    <td>Creates a camera and returns the camera index on success, -1 on failure.</td>
  </tr>
  <tr>
    <td>bool set_camera_parameter(int index, CameraConfigure & param)</td>
    <td>index: camera index, param: camera parameters</td>
    <td>Returns true when the camera parameters are set successfully.</td>
  </tr>
  <tr>
    <td>bool start_camera(int index)</td>
    <td>index: camera index</td>
    <td>Starts the camera. Returns true when started successfully.</td>
  </tr>
  <tr>
    <td>void stop_camera(int index)</td>
    <td>index: camera index</td>
    <td>Stop the camera.</td>
  </tr>
  <tr>
    <td>bool register_callback(int index, ImageCallback image_cb)</td>
    <td>index: camera index, image_cb: callback invoked for each new frame</td>
    <td>Registers the callback for image frames. The callback receives a <code>std::unique_ptr&lt;CameraFrame&gt;</code> per frame.</td>
  </tr>
  <tr>
    <td>bool destroy_camera(int index)</td>
    <td>index: camera index</td>
    <td>Stops and destroys the camera. Returns true on success.</td>
  </tr>
  <tr>
    <td>std::vector&lt;CameraEntry&gt; list_cameras()</td>
    <td>-</td>
    <td>Returns the cameras reported by the loaded backends (backend name, camera id, description).</td>
  </tr>
  <tr>
    <td>int64_t device_now_ns(int index)</td>
    <td>index: camera index</td>
    <td>Returns the current time of the clock used by the camera's frame timestamps, in ns; -1 for an invalid index.</td>
  </tr>
</table>

> [!Note]
> The parameter values should be set according to the actual camera hardware capabilities.

## 🎯 Supported targets

<table>
  <tr>
    <th>Development Hardware</th>
    <td>RRD</td>
  </tr>
  <tr>
    <th>GMSL Camera Support</th>
    <td><li>LI-VENUS-OX03F10-96717-120H</li></td>
  </tr>
</table>

---

## 🚀 Usage
### Check whether the camera devices have been detected and obtain the corresponding camera_id
The cameras each loaded backend reports are listed in the start-up log of the node:
```bash
[CameraManager]: camera backends (config): <backend> [<camera ids>]; default: <backend>
```
For the qcarcam backend, the inputIds can also be found in the system log:
```bash
journalctl -b |grep -i input_
```

### Start the camera node
```bash
source /opt/ros/jazzy/setup.bash
ros2 launch qrb_ros_camera qrb_ros_camera_launch.py
```

When using this launch script, it will use the default parameters:

```py
parameters=[{
    'camera_backend': '',
    'camera_id': 0,
    'stream_size': 1,
    'stream_name': ['stream1'],
    'stream1.height': 720,
    'stream1.width':  1280,
    'stream1.fps':    30,
    'camera_info_path': os.path.join(
        get_package_share_directory('qrb_ros_camera'),
        'config', 'camera_info_OX03F10_yuv.yaml'),
}]
```

It opens camera `0` on the default backend of the plugin_discovery configuration, with `1` stream, using a resolution of `1280 x 720`, and outputs image at `30` Hz.

You can override defaults via launch arguments:

```bash
ros2 launch qrb_ros_camera qrb_ros_camera_launch.py camera_id:=6 width:=1824 height:=1536 fps:=30
```

The output for these commands:

```bash
[INFO] [launch]: All log files can be found below /home/ubuntu/.ros/log/...
[INFO] [launch]: Default logging verbosity is set to INFO
[INFO] [component_container-1]: process started with pid [...]
[component_container-1] [INFO] [...] [camera_container]: Load Library: /opt/ros/jazzy/lib/libcamera_node.so
[component_container-1] [INFO] [...] [camera_node]: QRB Camera Node start
[component_container-1] [INFO] [...] [camera_node]: load camera intrinsic param
[component_container-1] [INFO] [CameraManager]: camera backends (config): qcarcam [6]; default: qcarcam
[component_container-1] [INFO] [CameraManager]: created camera 6 on backend 'qcarcam'
[component_container-1] [INFO] [...] [camera_node]: camera 6 on backend 'qcarcam'
[component_container-1] [INFO] [...] [camera_node]: camera clock: ... ros time: ... time offset: ... ns
[component_container-1] [INFO] [CameraManager]: started camera 6 on backend 'qcarcam'
[component_container-1] [INFO] [...] [camera_node]: QRB Camera Node init success
...
```

### Select the camera backend

By default the node uses the first backend of the plugin_discovery configuration. The platform configuration comes with the device (`/etc/plugin_discovery/platform.d/camera.config.yaml`); a customer configuration in `/etc/plugin_discovery/config.d/` applies after it. Check the result with:

```bash
pluginlib-detect -d camera
```

To add, reorder or disable backends, put `NN-camera-<source>.config.yaml` into `/etc/plugin_discovery/config.d/`, for example:

```yaml
domains:
  camera:
    backends: [my_camera]
    disable: [qmmf]
```

`reset: true` replaces the whole list. A customer backend with the same backend name replaces the Qualcomm one. The rules are in `docs/TEMPLATES.md` of plugin_discovery.

To name the backend by hand (only this one is loaded, the configuration is ignored):

```bash
ros2 launch qrb_ros_camera qrb_ros_camera_launch.py camera_backend:=qcarcam camera_id:=6
PLUGIN_DISCOVERY_BACKEND_CAMERA=qcarcam ros2 launch qrb_ros_camera qrb_ros_camera_launch.py camera_id:=6
```

A customer backend depends on the interface only:

```cmake
find_package(qrb_camera 3.0 REQUIRED)
add_library(my_camera SHARED my_camera.cpp)       # implements qrb_camera::ICameraBackend
target_link_libraries(my_camera PRIVATE qrb_camera::camera_interface)
plugin_discovery_add_plugin(TARGET my_camera DOMAIN camera NAME camera/MyCamera
  CLASS my::MyCamera BASE qrb_camera::ICameraBackend BACKEND my_camera)
```

Then you can check ROS topics or view image with the topic `/cam${camera_id}_${stream_name}` in RVIZ or RQT.

```bash
ros2 topic list
/cam0_stream1
/cam0_stream1_camera_info
```

### Enable zero copy transport

The `qrb_ros_camera` supports directly sharing image `dmabuf_fd` between nodes, which can avoid image data memory copy with DDS.

For detail about this feature, see https://docs.ros.org/en/rolling/Concepts/Intermediate/About-Composition.html

We recommend using `launch` to compose multiple nodes:

```python
def generate_launch_description():
    container = ComposableNodeContainer(
        name='camera_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container',
        composable_node_descriptions=[
            ComposableNode(
                package='qrb_ros_camera',
                plugin='qrb_ros::camera::CameraNode',
                name='camera_node',
                parameters=[{
                    # ...
                }]
            ),
            ComposableNode(
                package='qrb_ros_camera',
                plugin='qrb_ros::camera::TestNode',
                name='test',
                remappings=[
                    ('/image',       '/cam0_stream1'),
                    ('/camera_info', '/cam0_stream1_camera_info'),
                ],
            )
        ],
        output='screen',
    )

    return launch.LaunchDescription([container])
```

> [!Note]
> TestNode exists only in a build with tests (`BUILD_TESTING=ON`). With the shipped launch file, load it with `enable_test_node:=true`.

---

## 👨‍💻 Build from source

```bash
cd <workspace>
./build-utils/ubuntu/build.py --gen-debians --package ros-jazzy-qrb-camera
./build-utils/ubuntu/build.py --gen-debians --package ros-jazzy-qrb-ros-camera
```
Built `.deb` files are output to:
```bash
<workspace>/debian_packages/oss/ros-jazzy-qrb-camera
<workspace>/debian_packages/oss/ros-jazzy-qrb-ros-camera
```

`ros-jazzy-qrb-camera` builds three packages: `ros-jazzy-qrb-camera` (the interface and the manager), `ros-jazzy-qrb-camera-backend-qmmf` and `ros-jazzy-qrb-camera-backend-qcarcam`. Building needs `plugin-discovery` (>= 0.3.0) and the vendor SDKs of both backends.

On the device, install `plugin-discovery`, `plugin-discovery-platform-config`, `ros-jazzy-qrb-camera`, the backend packages of the device's camera stack, and `ros-jazzy-qrb-ros-camera`:
```bash
sudo dpkg -i plugin-discovery_*.deb plugin-discovery-platform-config_*.deb
sudo dpkg -i ros-jazzy-qrb-camera_*.deb ros-jazzy-qrb-camera-backend-<backend>_*.deb ros-jazzy-qrb-ros-camera_*.deb
```

> [!Note]
> Install local debs with `dpkg -i`. After `apt install`, needrestart may restart platform services, and on some images the camera service does not come back. If apt is needed, use `NEEDRESTART_MODE=l`.

Check whether the camera devices have been detected and obtain the corresponding camera_id: see [Usage](#-usage).

Run and debug
```bash
source /opt/ros/jazzy/setup.bash
ros2 launch qrb_ros_camera qrb_ros_camera_launch.py camera_id:=<camera_id> width:=<width> height:=<height> fps:=<fps>
```

## 📜 License

Project is licensed under the [BSD-3-Clause](https://spdx.org/licenses/BSD-3-Clause.html) License. See [LICENSE](./LICENSE) for the full license text.
