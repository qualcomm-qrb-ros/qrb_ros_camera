## 👋 Overview

The `QRB ROS Camera` is a ROS package to publish images from Qualcomm cameras. It provides:

- Concurrent multiple streams output support.
- Composable ROS node support.
- Zero-Copy transport powered by `QRB ROS Transport`.
- Supports multiple output formats: `nv12`, `mipi16` (16UC1), `mipi8` (mono8).


<div align="center">
  <img src="./docs/assets/architecture.png" alt="architecture">
</div>

<br>

The `qrb_ros_camera` is a ROS 2 package. It creates an image publisher with qrb_ros_transport for zero-copy transport. It supports node composition, making it possible to improve performance using ROS intra-process communication.

The `qrb_camera` is a C++ library, it provides APIs to `qrb_ros_camera` for querying images from the lower layer `QCarCam` library. It includes 2 modules:
- The module `camera_manager` used to manage camera streams, which enables multiple stream support.
- The module `qcarcam_camera` used to call QCarCam APIs to manage camera streams and handle frame delivery via DMA-heap buffers.

The `qrb_ros_transport` is a ROS 2 package, it supports zero-copy image transport with Linux DMA buffer and implements ROS type adaption, make it compatible with both intra- and inter-process communication.

The `QCarCam` (Qualcomm Camera Client) is the Qualcomm camera framework, it exports APIs for accessing Qualcomm camera hardware via the QCX camera service.

## 🔎 Table of contents
  * [APIs](#-apis)
     * [`qrb_ros_camera` APIs](#-qrb_ros_camera-apis)
     * [`qrb_camera` APIs](#-qrb_camera-apis)
  * [Supported targets](#-supported-targets)
  * [Installation](#-installation)
  * [Usage](#-usage)
     * [Starting the camera node](#start-the-camera-node)
     * [Enable mutiple streams](#enable-multiple-streams)
     * [Enable zero copy transport](#enable-zero-copy-transport)
  * [Build from source](#-build-from-source)
  * [Contributing](#-contributing)
  * [Contributors](#%EF%B8%8F-contributors)
  * [FAQs](#-faqs)
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
    <th>Description</th>
    <th>Default Value</th>
  </tr>
  <tr>
    <td>camera_id</td>
    <td>int</td>
    <td>The camera device ID (QCarCam inputId)</td>
    <td>0</td>
  </tr>
  <tr>
    <td>isp_usecase</td>
    <td>int</td>
    <td>ISP usecase passed to QCarCam. 0 = PREVIEW; >=256 = custom usecase</td>
    <td>0</td>
  </tr>
  <tr>
    <td>stream_size</td>
    <td>int</td>
    <td>Number of camera streams</td>
    <td>1</td>
  </tr>
  <tr>
    <td>stream_name</td>
    <td>string[]</td>
    <td>Names of each stream; used as the key prefix for per-stream parameters</td>
    <td>["stream0"]</td>
  </tr>
  <tr>
    <td>${stream_name}.width</td>
    <td>int</td>
    <td>Image width in pixels</td>
    <td>1920</td>
  </tr>
  <tr>
    <td>${stream_name}.height</td>
    <td>int</td>
    <td>Image height in pixels</td>
    <td>1080</td>
  </tr>
  <tr>
    <td>${stream_name}.fps</td>
    <td>int</td>
    <td>Output frame rate (Hz)</td>
    <td>30</td>
  </tr>
  <tr>
    <td>${stream_name}.format</td>
    <td>string</td>
    <td>Stream format. <code>nv12</code> -> ROS encoding <code>nv12</code>; <code>mipi16</code> -> <code>16UC1</code>; <code>mipi8</code> -> <code>mono8</code></td>
    <td>nv12</td>
  </tr>
  <tr>
    <td>${stream_name}.bufferlist_id</td>
    <td>int</td>
    <td>QCarCam bufferlist id for this stream. Must match the value in the camera XML config. Each stream must have a unique non-zero id.</td>
    <td>(required, no default)</td>
  </tr>
  <tr>
    <td>${stream_name}.camera_info_path</td>
    <td>string</td>
    <td>Absolute path to the calibration YAML file for this stream. Leave empty to publish empty intrinsics.</td>
    <td>""</td>
  </tr>
</table>

> [!Note]
> The parameter values should be set according to the actual camera hardware capabilities.

### 🔹 `qrb_camera` APIs

<table>
  <tr>
    <th>Function</th>
    <th>Parameters</th>
    <th>Description</th>
  </tr>
  <tr>
    <td>int create_camera(CameraType type, uint32_t camera_id)</td>
    <td>type: camera type, camera_id: camera id</td>
    <td>Creates a camera and returns the camera index on success.</td>
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
```bash
journalctl -b |grep -i input_
```

### Start the camera node

**Option A : `ros2 run` (no launch file needed):**

```bash
source /opt/ros/jazzy/setup.bash
ros2 run qrb_ros_camera camera_node_exe \
    --ros-args --params-file /path/to/camera.yaml
```

The config example:

```yaml
/**:
  ros__parameters:
    camera_id: 4
    isp_usecase: 0

    stream_size: 3
    stream_name:
      - stream0
      - stream1
      - stream2

    # Depth
    stream0:
      width:  1280
      height: 720
      fps:    30
      format: mipi16
      bufferlist_id: 1
      camera_info_path: ""

    # Color
    stream1:
      width:  1280
      height: 720
      fps:    30
      format: nv12
      bufferlist_id: 2
      camera_info_path: ""

    # IR
    stream2:
      width:  1280
      height: 720
      fps:    30
      format: mipi8
      bufferlist_id: 3
      camera_info_path: ""
```

**Option B : `ros2 launch` (single stream):**

```bash
source /opt/ros/jazzy/setup.bash
ros2 launch qrb_ros_camera qrb_ros_camera_launch.py \
    camera_id:=0 bufferlist_id:=5 width:=1280 height:=720 fps:=30 format:=nv12
```

**Option C : `ros2 launch` (multi-stream via YAML config):**

```bash
source /opt/ros/jazzy/setup.bash
ros2 launch qrb_ros_camera multi_stream_launch.py \
    camera_config:=/path/to/camera.yaml
```

Then you can check ROS topics or view images with the topic `/cam${camera_id}_${stream_name}` in RVIZ or RQT.

```bash
ros2 topic list
/cam0_stream0
/cam0_stream0_camera_info
```

### Format and ROS encoding mapping

| `format` parameter | ROS image encoding | Typical use |
|---|---|---|
| `nv12` | `nv12` | Color (ISP output) |
| `mipi16` | `16UC1` | Depth (16-bit RAW) |
| `mipi8` | `mono8` | IR (8-bit RAW) |

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
                parameters=[camera_config],
            ),
            ComposableNode(
                package='qrb_ros_camera',
                plugin='qrb_ros::camera::TestNode',
                name='test',
                remappings=[
                    ('/image',       '/cam0_stream0'),
                    ('/camera_info', '/cam0_stream0_camera_info'),
                ],
            )
        ],
        output='screen',
    )

    return launch.LaunchDescription([container])
```

---

## 👨‍💻 Build from source

```bash
cd <workspace>
source /opt/ros/jazzy/setup.bash
colcon build
```

Check whether the camera devices have been detected and obtain the corresponding camera_id
```bash
journalctl -b |grep -i input_
```
Run and debug
```bash
source /opt/ros/jazzy/setup.bash
ros2 launch qrb_ros_camera qrb_ros_camera_launch.py \
    camera_id:=<camera_id> bufferlist_id:=<bufferlist_id> \
    width:=<width> height:=<height> fps:=<fps> format:=<format>
```

## 📜 License

Project is licensed under the [BSD-3-Clause](https://spdx.org/licenses/BSD-3-Clause.html) License. See [LICENSE](./LICENSE) for the full license text.
