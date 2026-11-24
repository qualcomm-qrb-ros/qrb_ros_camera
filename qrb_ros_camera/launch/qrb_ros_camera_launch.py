# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear

# One launch file for every camera backend. The camera backend comes from the
# plugin_discovery configuration of the machine; `camera_backend` overrides it.
#
#   ros2 launch qrb_ros_camera qrb_ros_camera_launch.py camera_id:=<id>
#   ros2 launch qrb_ros_camera qrb_ros_camera_launch.py camera_backend:=qcarcam camera_id:=<id>
#
# camera_info_path defaults to the example calibration shipped in share/qrb_ros_camera/config
# (as in the IQ10 launch file). It only fills the camera_info topic; pass the calibration of
# the connected sensor when the intrinsics matter.

import os

import launch
from ament_index_python.packages import get_package_share_directory
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import ComposableNodeContainer, LoadComposableNodes
from launch_ros.descriptions import ComposableNode
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    config_dir = os.path.join(get_package_share_directory('qrb_ros_camera'), 'config')

    # ── Launch arguments ────────────────────────────────────────────────────
    args = [
        DeclareLaunchArgument(
            'camera_backend', default_value='',
            description='Camera backend, e.g. qmmf or qcarcam. Empty: the default of the '
                        'plugin_discovery configuration (see pluginlib-detect -d camera)'),
        DeclareLaunchArgument(
            'camera_id', default_value='0',
            description="The backend's camera id, e.g. the QMMF camera id or the QCarCam inputId"),
        DeclareLaunchArgument('width', default_value='1280', description='Stream width in pixels'),
        DeclareLaunchArgument('height', default_value='720', description='Stream height in pixels'),
        DeclareLaunchArgument('fps', default_value='30', description='Target frame rate'),
        DeclareLaunchArgument(
            'camera_info_path',
            default_value=os.path.join(config_dir, 'camera_info_OX03F10_yuv.yaml'),
            description='Absolute path to the camera intrinsic YAML (default: the example in '
                        'share/qrb_ros_camera/config; it only fills the camera_info topic)'),
        DeclareLaunchArgument(
            'enable_test_node', default_value='false',
            description='Also load TestNode (fps / latency of the stream; needs a build with tests)'),
        DeclareLaunchArgument(
            'dump', default_value='False', description='TestNode: dump received frames to disk'),
    ]

    camera_backend = LaunchConfiguration('camera_backend')
    camera_id = LaunchConfiguration('camera_id')
    width = LaunchConfiguration('width')
    height = LaunchConfiguration('height')
    fps = LaunchConfiguration('fps')
    camera_info_path = LaunchConfiguration('camera_info_path')
    enable_test_node = LaunchConfiguration('enable_test_node')
    dump = LaunchConfiguration('dump')

    image_topic = PythonExpression(["'/cam' + '", camera_id, "' + '_stream1'"])
    camera_info_topic = PythonExpression(["'/cam' + '", camera_id, "' + '_stream1_camera_info'"])

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
                    # str: an empty value must stay '' (YAML would turn it into null)
                    'camera_backend':   ParameterValue(camera_backend, value_type=str),
                    'camera_id':        PythonExpression(["int('", camera_id, "')"]),
                    'stream_size':      1,
                    'stream_name':      ['stream1'],
                    'stream1.height':   PythonExpression(["int('", height, "')"]),
                    'stream1.width':    PythonExpression(["int('", width, "')"]),
                    'stream1.fps':      PythonExpression(["int('", fps, "')"]),
                    'camera_info_path': camera_info_path,
                }],
            ),
        ],
        output='screen',
    )

    # Optional, and off by default: it subscribes to the camera's own topic.
    test_node = LoadComposableNodes(
        target_container='camera_container',
        condition=IfCondition(enable_test_node),
        composable_node_descriptions=[
            ComposableNode(
                package='qrb_ros_camera',
                plugin='qrb_ros::camera::TestNode',
                name='test',
                remappings=[
                    ('/image', image_topic),
                    ('/camera_info', camera_info_topic),
                ],
                parameters=[{
                    'dump':             dump,
                    'dump_camera_info': False,
                }],
            ),
        ],
    )

    return launch.LaunchDescription(args + [container, test_node])
