// Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#ifndef TEST_ROS2_NODE_HPP_
#define TEST_ROS2_NODE_HPP_

#include <stdio.h>

#include "qrb_ros_camera/camera_ros2_common.hpp"
#include "qrb_ros_camera/camera_ros2_node.hpp"
#include "qrb_ros_transport_image_type/image.hpp"
#include "rclcpp/rclcpp.hpp"

#define IMAGE_TOPIC_NAME "image"
#define CAMERA_INFO_TOPIC_NAME "camera_info"
#define IMAGE_TOPIC_TYPE qrb_ros::transport::type::Image
#define CAMERA_INFO_TOPIC_TYPE sensor_msgs::msg::CameraInfo

namespace qrb_ros::camera {
class TestNode : public rclcpp ::Node {
public:
  explicit TestNode(const rclcpp::NodeOptions &options);
  explicit TestNode(const std::string &name,
                    const rclcpp::NodeOptions &options);
  int64_t cal_latency(int64_t sec, int64_t nsec);
  int64_t get_cur_time();
  void show_receive_fps(CameraRos2Frame *frame);

private:
  void handler_callback(const qrb_ros::transport::type::Image &handler);
  void camera_info_callback(const sensor_msgs::msg::CameraInfo &camera_info);
  std::shared_ptr<rclcpp::Subscription<qrb_ros::transport::type::Image>>
      handler_sub_;
  std::shared_ptr<rclcpp::Subscription<sensor_msgs::msg::CameraInfo>>
      camera_info_sub_;
  std::unique_ptr<CameraRos2Frame> m_p_frame_;
  std::shared_ptr<sensor_msgs::msg::CameraInfo> m_p_camera_info_;
  int64_t g_receive_last_fpstime;  // only add this for test fps
  int64_t g_receive_total_latency; // only add this for test fps
  uint32_t g_receive_frame_count;  // only add this for test fps
  bool dump_ = false;
  bool dump_camera_info_ = false;
};
} // namespace qrb_ros::camera

#endif