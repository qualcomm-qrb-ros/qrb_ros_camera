# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear

import launch
import os
from ament_index_python.packages import get_package_share_directory
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PythonExpression


def generate_launch_description():
    config_dir = get_package_share_directory('qrb_ros_camera')

    # ── Launch arguments ────────────────────────────────────────────────────
    camera_id_arg = DeclareLaunchArgument(
        'camera_id', default_value='0',
        description='Camera ID: inputId for QCarCam (e.g. 11/6)')

    width_arg  = DeclareLaunchArgument('width',  default_value='1280', description='Stream width in pixels')
    height_arg = DeclareLaunchArgument('height', default_value='720',  description='Stream height in pixels')
    fps_arg    = DeclareLaunchArgument('fps',    default_value='30',   description='Target frame rate')

    camera_info_path_arg = DeclareLaunchArgument(
        'camera_info_path',
        default_value=os.path.join(config_dir, 'config', 'camera_info_OX03F10_yuv.yaml'),
        description='Absolute path to camera intrinsic YAML file')

    dump_arg = DeclareLaunchArgument(
        'dump', default_value='False',
        description='Dump received frames to disk (debug)')

    # ── Substitution handles ────────────────────────────────────────────────
    camera_id        = LaunchConfiguration('camera_id')
    width            = LaunchConfiguration('width')
    height           = LaunchConfiguration('height')
    fps              = LaunchConfiguration('fps')
    camera_info_path = LaunchConfiguration('camera_info_path')
    dump             = LaunchConfiguration('dump')

    image_topic       = PythonExpression(["'/cam' + '", camera_id, "' + '_stream1'"])
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
                    'camera_id':        PythonExpression(["int('", camera_id, "')"]),
                    'stream_size':      1,
                    'stream_name':      ['stream1'],
                    'stream1.height':   PythonExpression(["int('", height, "')"]),
                    'stream1.width':    PythonExpression(["int('", width,  "')"]),
                    'stream1.fps':      PythonExpression(["int('", fps,    "')"]),
                    'stream1.camera_info_path': camera_info_path,
                }],
            ),
            ComposableNode(
                package='qrb_ros_camera',
                plugin='qrb_ros::camera::TestNode',
                name='test',
                remappings=[
                    ('/image',       image_topic),
                    ('/camera_info', camera_info_topic),
                ],
                parameters=[{
                    'dump':             dump,
                    'dump_camera_info': False,
                }],
            ),
        ],
        output='screen',
    )

    return launch.LaunchDescription([
        camera_id_arg,
        width_arg,
        height_arg,
        fps_arg,
        camera_info_path_arg,
        dump_arg,
        container,
    ])
