// Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#ifndef CAMERA_ROS2_NODE_HPP_
#define CAMERA_ROS2_NODE_HPP_

#include <yaml-cpp/yaml.h>

#include <sensor_msgs/msg/camera_info.hpp>

#include "qmmf_ros2_pipeline.hpp"
#include "qrb_ros_camera/camera_ros2_common.hpp"
#include "qrb_ros_camera/camera_ros2_config.hpp"
#include "qrb_ros_transport_image_type/image.hpp"
#include "rclcpp/rclcpp.hpp"

#define CAMERA_ROS2_FRAME_ID "cam[%u_%u] id[%u]"
#define IMAGE_TOPIC_NAME "image"
#define CAMERA_INFO_TOPIC_NAME "camera_info"
#define IMAGE_TOPIC_TYPE qrb_ros::transport::type::Image
#define CAMERA_INFO_TOPIC_TYPE sensor_msgs::msg::CameraInfo

namespace qrb_ros::camera
{
class CameraNode : public rclcpp::Node
{
public:
  explicit CameraNode(const rclcpp::NodeOptions & options);
  ~CameraNode();
  void load_config();
  void load_camerainfo();
  int64_t cal_latency(int64_t ts);
  int64_t get_cur_ros_time();
  int64_t get_cur_time();
  void SyncTimestamp();
  void show_fps(CameraRos2Frame * frame);

private:
  bool init();
  void publish_image(CameraRos2Frame * frame);
  void get_image_info(CameraRos2Frame * frame,
      std::unique_ptr<qrb_ros::transport::type::Image> & img_msg);
  void convert_to_camerainfo(const YAML::Node & yaml_node);
  std::shared_ptr<rclcpp::Publisher<qrb_ros::transport::type::Image>> handler_pub_;
  std::shared_ptr<rclcpp::Publisher<sensor_msgs::msg::CameraInfo>> camera_info_pub_;
  CameraRos2Config * m_p_config_;  // record parameters
  QmmfRos2Pipeline * m_p_pipeline_;
  std::string camera_info_url_;
  int64_t time_offset_;
  int64_t g_last_fpstime;
  int64_t g_total_latency;
  uint32_t g_frame_count;
  std::shared_ptr<sensor_msgs::msg::CameraInfo> m_p_camera_info_;
  CameraRos2MsgPublishFrameFunc m_publish_frame_function_;
};
}  // namespace qrb_ros::camera

#endif