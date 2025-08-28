<div align="center">
  <h1>QRB ROS Camera</h1>
  <p align="center">
   <img src="https://s7d1.scene7.com/is/image/dmqualcommprod/rb3gen2-dev-kits-hero-7" alt="Qualcomm QRB ROS" title="Qualcomm QRB ROS" />
      
  </p>
  <p>ROS Packages for Cameras on Qualcomm Robotics Platforms</p>
  
  <a href="https://ubuntu.com/download/qualcomm-iot" target="_blank"><img src="https://img.shields.io/badge/Qualcomm%20Ubuntu-E95420?style=for-the-badge&logo=ubuntu&logoColor=white" alt="Qualcomm Ubuntu"></a>
  <a href="https://docs.ros.org/en/jazzy/" target="_blank"><img src="https://img.shields.io/badge/ROS%20Jazzy-1c428a?style=for-the-badge&logo=ros&logoColor=white" alt="Jazzy"></a>
  
</div>

---

## 👋 Overview

The [QRB ROS Camera](https://github.com/qualcomm-qrb-ros/qrb_ros_camera) is a ROS package to publish the images from Qualcomm **CSI** and **GMSL** cameras. It provides:

- Concurrent multiple streams output support.
- Composable ROS node support.
- Zero-Copy transport powered by [QRB ROS Transport](https://github.com/qualcomm-qrb-ros/qrb_ros_transport).
- Only support outputs in NV12 format，limited by the Camera Service.


<div align="center">
  <img src="./docs/assets/architecture.png" alt="architecture">
</div>

<br>

The [`qrb_ros_camera`](https://github.com/qualcomm-qrb-ros/qrb_ros_camera/tree/main/qrb_ros_camera) is a ROS 2 package. It creates an image publisher with qrb_ros_transport for zero-copy transport. It supports node composition, making it possible to improve performance using ROS intra-process communication.

The [`qrb_camera`](https://github.com/qualcomm-qrb-ros/qrb_ros_camera/tree/main/qrb_camera) is a C++ library, it provides APIs to `qrb_ros_camera` for querying images from lower layer `Camera Service` and `CamX` libraries. It includes 2 modules:
- The module `camera_manager` used to manage camera stream, which enables multiple stream support.
- The module `camera_client` used to call `Camera Service` apis to manage camera streams.

The [`qrb_ros_transport`](https://github.com/qualcomm-qrb-ros/qrb_ros_transport) is a ROS 2 package, it supports zero-copy image transport with Linux DMA buffer and implements ROS type adaption, make it compatible with both intra- and inter-process communication. 

The `Camera Service` is Qualcomm multimedia framework, it exports APIs for accessing Qualcomm multimedia hardware.

The `CamX` provides the foundation for image capture, processing, and management on Qualcomm-powered devices.

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
    <td>sensor_msgs/msg/Image</td>
    <td>output image</td>
  </tr>
  <tr>
    <td></td>
    <td>/cam${camera_id}_camera_info</td>
    <td>sensor_msgs/msg/CameraInfo</td>
    <td>camera information</td>
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
    <td>camera_id</td>
    <td>int64</td>
    <td>The camera device ID</td>
    <td>0</td>
  </tr>
  <tr>
    <td>stream_size</td>
    <td>uint64</td>
    <td>Count of camera stream</td>
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
    <td>1920</td>
  </tr>  
  <tr>
    <td>${stream_name}.height</td>
    <td>uint32</td>
    <td>image height</td>
    <td>1080</td>
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
    <td>Camera metadata file path</td>
    <td>config/camera_info_imx577.yaml</td>
  </tr>
</table>

> [!Note]
> The parameter values should be set accroding to the actual camera hardware capabilities.

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
    <td>index: camera index,  param: camera parameters</td>
    <td>Returns true when the camera parameters are set successfully.</td>
  </tr>
  <tr>
    <td>bool start_camera(int index)</td>
    <td>index: camera index </td>
    <td>Starts the camera. Returns true when started successfully.</td>
  </tr>
  <tr>
    <td>void stop_camera(int index)</td>
    <td>index: camera index </td>
    <td>Stop the camera</td>
  </tr>
  <tr>
    <td>bool register_callback(int index, ImageCallback image_cb, PointCloudCallback point_cloud_cb)</td>
    <td>index: camera index，Image_cb: image callback，Point_cloud_cb: point cloud msg callback </td>
    <td>Registers the callback for image and point cloud messages.</td>
  </tr>
</table>

> [!Note]
> The parameter values should be set accroding to the actual camera hardware capabilities.

## 🎯 Supported targets

<table >
  <tr>
    <th>Development Hardware</th>
    <td>Qualcomm Dragonwing™ RB3 Gen2</td>
    <td>Qualcomm Dragonwing™ IQ-9075 EVK</td>
  </tr>
  <tr>
    <th>Hardware Overview</th>
    <th><a href="https://www.qualcomm.com/developer/hardware/rb3-gen-2-development-kit"><img src="https://s7d1.scene7.com/is/image/dmqualcommprod/rb3-gen2-carousel?fmt=webp-alpha&qlt=85" width="180"/></a></th>
    <th><a href="https://www.qualcomm.com/products/internet-of-things/industrial-processors/iq9-series/iq-9075"><img src="https://s7d1.scene7.com/is/image/dmqualcommprod/dragonwing-IQ-9075-EVK?$QC_Responsive$&fmt=png-alpha" width="160"></a></th>
  </tr>
  <tr>
    <th>MIPI-CSI Camera Support</th>
    <td><li>IMX577(12MP)</li><li>OV9282(1MP)</li></td>
    <td><li>IMX577(12MP)</li><li>OV9282(1MP)</li></td>
  </tr>
  <tr>
    <th>GMSL Camera Support</th>
    <td>Leopard Imaging AR0231 GMSL2</td>
    <td><li>LI-VENUS-OX03F10-96717-120H(Bayer)</li><li>LI-VENUS-OX03F10-OAX40-GM2A-118H(YUV)</li></td>
  </tr>
</table>

---

## ✨ Installation

> [!IMPORTANT]
> **PREREQUISITES**: The following steps need to be run on **Qualcomm Ubuntu** and **ROS Jazzy**.<br>
> Reference [Install Ubuntu on Qualcomm IoT Platforms](https://ubuntu.com/download/qualcomm-iot) and [Install ROS Jazzy](https://docs.ros.org/en/jazzy/index.html) to setup environment. <br>
> For Qualcomm Linux, please check out the [Qualcomm Intelligent Robotics Product SDK](https://docs.qualcomm.com/bundle/publicresource/topics/80-70018-265/introduction_1.html?vproduct=1601111740013072&version=1.4&facet=Qualcomm%20Intelligent%20Robotics%20Product%20(QIRP)%20SDK) documents.

Add Qualcomm IOT PPA for Ubuntu:

```bash
sudo add-apt-repository ppa:ubuntu-qcom-iot/qcom-noble-ppa
sudo add-apt-repository ppa:ubuntu-qcom-iot/qirp
sudo apt update
```

Install Debian package:

```bash
sudo apt install ros-jazzy-qrb-ros-camera
```

## 🚀 Usage

### Start the camera node

```bash
source /opt/ros/jazzy/setup.bash
ros2 launch qrb_ros_camera qrb_ros_camera_launch.py
```

When using this launch script, it will use the default parameters:

```py
 parameters=[{
    'camera_id': 0,
    'stream_size': 1,
    'stream_name': ["stream1"],
    'stream1':{
        'height':1080,
        'width':1920,
        'fps':30,
    },
    'camera_info_path': os.path.join(
        get_package_share_directory('qrb_ros_camera'),
        'config', 'camera_info_imx577.yaml'),
}]
```

It opens camera `0`, with `1` stream, using a resolution of `1920 x 1080`, and outputs image at `30` Hz. 

The output for these commands:

```bash
[INFO] [launch]: All log files can be found below /home/ubuntu/.ros/log/2025-07-11-08-33-27-770692-ubuntu-13833
[INFO] [launch]: Default logging verbosity is set to INFO
/opt/ros/jazzy/share/qrb_ros_camera/config/camera_info_imx577.yaml
[INFO] [component_container_mt-1]: process started with pid [788455]
[component_container_mt-1] [INFO] [1738523640.352029048] [my_container]: Load Library: /opt/ros/jazzy/lib/libcamera_node.so
[component_container_mt-1] [INFO] [1738523640.391541131] [my_container]: Found class: rclcpp_components::NodeFactoryTemplate<qrb_ros::camera::CameraNode>
[component_container_mt-1] [INFO] [1738523640.391636496] [my_container]: Instantiate class: rclcpp_components::NodeFactoryTemplate<qrb_ros::camera::CameraNode>
[component_container_mt-1] [INFO] [1738523640.407687277] [cam0_node]: QRB Camera Node statrt
[component_container_mt-1] [INFO] [1738523640.408260298] [cam0_node]: load camera intrinsic param
[component_container_mt-1] [INFO] [1738523640.422302798] [cam0_node]: system time: 263304942972212 ros time: 1738523640422300871 time offset: 1738260335479328659 ns
[component_container_mt-1] [INFO] [1738523640.424454465] [cam0_node]: QRB Camera Node init success
...
```

Then you can check ROS topics or view image with the topic `/cam${camera_id}_${stream_name}` in RVIZ or RQT.

```bash
ros2 topic list
/cam0_stream1
/cam0_camera_info
```

### Enable multiple streams

By using the `stream_size` and `stream_name` parameters, you can configure multiple streams for one camera.

```bash
 parameters=[{
    'camera_id': 0,
    'stream_size': 2,
    'stream_name': ["stream1", "stream2"],
    'stream1':{
        'width':1920,
        'height':1080,
        'fps':30,
    },
    'stream2':{
        'width':1080,
        'height':720,
        'fps':60,
    },
    'camera_info_path': os.path.join(
        get_package_share_directory('qrb_ros_camera'),
        'config', 'camera_info_imx577.yaml'),
}]
```

### Enable zero copy transport

The `qrb_ros_camera` supports directly sharing image `dmabuf_fd` between nodes, which can avoid image data memory copy with DDS.

For detail about this feature, see https://docs.ros.org/en/rolling/Concepts/Intermediate/About-Composition.html

We recommend using `launch` to compose multiple nodes:

```python
def generate_launch_description():
    container = ComposableNodeContainer(
        name='my_container',
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
                name='sub_node',
            )
        ],
        output='screen',
    )

    return launch.LaunchDescription([container])
```

---

## 👨‍💻 Build from source

### Dependencies
Install dependencies `ros-dev-tools`

```bash
sudo apt install ros-jazzy-qrb-ros-transport-image-type \
  ros-dev-tools
```

### Build
Download the source code and build with colcon
```bash
source /opt/ros/jazzy/setup.bash
git clone https://github.com/qualcomm-qrb-ros/qrb_ros_camera.git
colcon build
```


## 🤝 Contributing

We love community contributions! Get started by reading our [CONTRIBUTING.md](CONTRIBUTING.md).<br>
Feel free to create an issue for bug report, feature requests or any discussion💡.

## ❤️ Contributors

Thanks to all our contributors who have helped make this project better!

<table>
  <tr>
    <td align="center"><a href="https://github.com/quic-zhanlin"><img src="https://avatars.githubusercontent.com/u/88314584?v=4" width="100" height="100" alt="quic-zhanlin"/><br /><sub><b>quic-zhanlin</b></sub></a></td>
    <td align="center"><a href="https://github.com/dingtian777"><img src="https://avatars.githubusercontent.com/u/154509668?v=4" width="100" height="100" alt="dingtian777"/><br /><sub><b>dingtian777</b></sub></a></td>
    <td align="center"><a href="https://github.com/jiaxshi"><img src="https://avatars.githubusercontent.com/u/147487233?v=4" width="100" height="100" alt="jiaxshi"/><br /><sub><b>jiaxshi</b></sub></a></td>
    <td align="center"><a href="https://github.com/quic-zhaoyuan"><img src="https://avatars.githubusercontent.com/u/164289792?v=4" width="100" height="100" alt="quic-zhaoyuan"/><br /><sub><b>quic-zhaoyuan</b></sub></a></td>
  </tr>
</table>

## ❔ FAQs

<details>
<summary>Why is RGB image format not supported?</summary><br>
NV12 format is the original format from the Qualcomm camera framework. If you need RGB images, you can use a color conversion node.
</details>

<details>
<summary>Does it support USB cameras?</summary><br>
No, it only supports MIPI-CSI and GMSL cameras, which are based on CamX.
</details>

## 📜 License

Project is licensed under the [BSD-3-Clause](https://spdx.org/licenses/BSD-3-Clause.html) License. See [LICENSE](./LICENSE) for the full license text.
