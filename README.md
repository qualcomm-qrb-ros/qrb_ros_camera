# QRB ROS CAMERA

## Overview

`qrb_ros_camera` is a ROS package to publish the camera data from camera sensor.

`qrb_ros_camera` holds significant importance as it serves as a critical component for numerous other ROS nodes that 
rely on image data for analysis and processing. By providing images in the NV12 format, it enables seamless integration 
with a wide range of ROS nodes, empowering advanced image-based analysis and functionalities within the ROS ecosystem.

`qrb_ros_camera` features zero-copy transport for reduced latency, facilitating swift image transmission. It also 
supports concurrent output from multiple cameras, enhancing system performance and scalability in real-time ROS 
applications.

## Getting Started

<details><summary>Cross Compile with QCLINUX SDK</summary>

#### Cross Compile with QCLINUX SDK

Setup QCLINUX SDK environments:
- Reference [QRB ROS Documents: Getting Started](https://quic-qrb-ros.github.io/getting_started/environment_setup.html)

Create workspace in QCLINUX SDK environment and clone source code

```bash
mkdir -p <qirp_decompressed_workspace>/qirp-sdk/ros_ws
cd <qirp_decompressed_workspace>/qirp-sdk/ros_ws
git clone https://github.com/quic-qrb-ros/qrb_ros_imu.git
git clone https://github.com/quic-qrb-ros/lib_mem_dmabuf.git
git clone https://github.com/quic-qrb-ros/qrb_ros_transport.git
git clone https://github.com/quic-qrb-ros/qrb_ros_camera.git
```

Build source code with QCLINUX SDK

```bash
export AMENT_PREFIX_PATH="${OECORE_TARGET_SYSROOT}/usr;${OECORE_NATIVE_SYSROOT}/usr"
export PYTHONPATH=${PYTHONPATH}:${OECORE_TARGET_SYSROOT}/usr/lib/python3.10/site-packages

colcon build --merge-install --cmake-args \
    -DPython3_ROOT_DIR=${OECORE_TARGET_SYSROOT}/usr \
    -DPython3_NumPy_INCLUDE_DIR=${OECORE_TARGET_SYSROOT}/usr/lib/python3.10/site-packages/numpy/core/include \
    -DSYSROOT_LIBDIR=${OECORE_TARGET_SYSROOT}/usr/lib \
    -DSYSROOT_INCDIR=${OECORE_TARGET_SYSROOT}/usr/include \
    -DPYTHON_SOABI=cpython-310-aarch64-linux-gnu -DCMAKE_STAGING_PREFIX=$(pwd)/install \
    -DCMAKE_PREFIX_PATH=$(pwd)/install/share \
    -DBUILD_TESTING=OFF
```

Install ROS package to device

```bash
cd <qirp_decompressed_workspace>/qirp-sdk/ros_ws/install
tar czvf qrb_ros_camera.tar.gz include lib share
scp qrb_ros_camera.tar.gz root@[ip-addr]:/opt/
ssh ssh root@[ip-addr]
(ssh) tar -zxf /opt/qrb_ros_camera.tar.gz -C /opt/qcom/qirp-sdk/usr/
```

Login to device and run

```bash
ssh root@[ip-addr]
(ssh) export HOME=/opt
(ssh) source /usr/bin/ros_setup.bash
(ssh) source /opt/qcom/qirp-sdk/qirp-setup.sh
(ssh) ros2 launch qrb_ros_camera qrb_ros_camera_launch.py
```

</details>

<details open><summary>Native Build on Ubuntu</summary>

#### Native Build on Ubuntu

Prerequisites

- ROS 2: [Install ROS2 on Ubuntu](https://docs.ros.org/en/humble/Installation/Ubuntu-Install-Debs.html)
- camera-server environment install:
```bash
sudo apt install libimage-transport-dev
sudo add-apt-repository ppa:carmel-team/jammy-release --login
sudo apt upgrade
sudo apt install weston-qcom gstreamer1.0-plugins-qcom gstreamer1.0-tools camxapi-kt libqmmf-dev syslog-plumber-dev
sudo cp /usr/include/log/log.h /usr/include/log.h
sudo reboot
```
Create workspace and clone source code from GitHub:

```bash
sudo -i
mkdir -p ~/ros2_ws/src && cd ~/ros2_ws/src
git clone https://github.com/quic-qrb-ros/qrb_ros_imu.git
git clone https://github.com/quic-qrb-ros/lib_mem_dmabuf.git
git clone https://github.com/quic-qrb-ros/qrb_ros_transport.git
git clone https://github.com/quic-qrb-ros/qrb_ros_camera.git
```
Build source code

```bash
cd ~/ros2_ws
source /opt/ros/${ROS_DISTRO}/setup.bash
colcon build
```

Run qrb_ros_camera

```bash
cd ~/ros2_ws
source install/setup.bash
ros2 launch qrb_ros_camera qrb_ros_camera_launch.py
```

</details>

## Supported Platforms

This package is designed and tested to be compatible with ROS 2 Humble running on Qualcomm RB3 gen2.

| Hardware                                                     | Software          |
| ------------------------------------------------------------ | ----------------- |
| [Qualcomm RB3 gen2](https://www.qualcomm.com/developer/hardware/rb3-gen-2-development-kit) | `LE.QCROBOTICS.1.0`, `Canonical Ubuntu Image for RB3 gen2` |

## Contributing

We would love to have you as a part of the QRB ROS community. Whether you are helping us fix bugs, proposing new features, improving our documentation, or spreading the word, please refer to our [contribution guidelines](./CONTRIBUTING.md) and [code of conduct](./CODE_OF_CONDUCT.md).

- Bug report: If you see an error message or encounter failures, please create a [bug report](../../issues)
- Feature Request: If you have an idea or if there is a capability that is missing and would make development easier and more robust, please submit a [feature request](../../issues)

## Authors

* **Tian Ding** - *Initial work* - [dingtian777](https://github.com/dingtian777)

See also the list of [contributors](https://github.com/your/project/contributors) who participated in this project.

## License

Project is licensed under the [BSD-3-clause License](https://spdx.org/licenses/BSD-3-Clause.html). See [LICENSE](./LICENSE) for the full license text.

