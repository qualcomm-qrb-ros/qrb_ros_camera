// Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#include "qrb_ros_camera/camera_ros2_node.hpp"
#include "rclcpp/rclcpp.hpp"

int32_t main(int32_t argc, char ** argv)
{
  RCLCPP_INFO(rclcpp::get_logger("qrb_ros_camera"), "camera ros2 node start running...");
  rclcpp::init(argc, argv);
  // using component need set options
  rclcpp::executors::SingleThreadedExecutor exec;
  rclcpp::NodeOptions options;
  options.use_intra_process_comms(true);
  // create cameraRosnode
  auto camera_node = std::make_shared<qrb_ros::camera::CameraNode>(options);
  exec.add_node(camera_node);
  exec.spin();
  rclcpp::shutdown();
  RCLCPP_INFO(rclcpp::get_logger("qrb_ros_camera"), "Camera Ros2 Node is exiting...");
  return 0;
}