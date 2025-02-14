# Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause-Clear

import launch
import os
from ament_index_python.packages import get_package_share_directory
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode

def generate_launch_description():
    camera_info_config_file_path = os.path.join(
        get_package_share_directory('qrb_ros_camera'),
        'config', 'camera_info_imx577.yaml'
    )
    camera_info_path = camera_info_config_file_path
    print(camera_info_path)
    """Generate launch description with multiple components."""
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
            'camera_info_path': camera_info_path,
            'fps': 30,
            'width': 1920,
            'height': 1080,
            'cameraId': 0,
        }]
        ),
    ComposableNode(
        package='qrb_ros_camera',
        plugin='qrb_ros::camera::TestNode',
        name='test',
        parameters=[{
            'dump': False,
            'dump_camera_info_': False,
        }]
        )
    ],
    output='screen',
    )

    return launch.LaunchDescription([container])