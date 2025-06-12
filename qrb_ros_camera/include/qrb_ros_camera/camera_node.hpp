/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef QRB_ROS_CAMERA__CAMERA_NODE_HPP_
#define QRB_ROS_CAMERA__CAMERA_NODE_HPP_

#include <yaml-cpp/yaml.h>

#include <sensor_msgs/msg/camera_info.hpp>

#include "qrb_camera/camera_manager.hpp"
#include "qrb_ros_camera/configure_parser.hpp"
#include "qrb_ros_transport_image_type/image.hpp"
#include "rclcpp/rclcpp.hpp"

#define IMAGE_TOPIC_TYPE qrb_ros::transport::type::Image
#define CAMERA_INFO_TOPIC_TYPE sensor_msgs::msg::CameraInfo

namespace qrb_ros
{
namespace camera
{
struct StreamTimeStamp
{
  std::string stream_name;
  uint64_t timestamp = 0;
  uint64_t frame_count = 0;
  int64_t latency = 0;
};

class CameraNode : public rclcpp::Node
{
public:
  ~CameraNode();

  explicit CameraNode(const rclcpp::NodeOptions & options);

private:
  void calculate_offset();

  void publish_image(std::unique_ptr<qrb_camera::CameraFrame> frame);

  void publish_color_image(int stream_index, std::unique_ptr<qrb_camera::CameraFrame> frame);

  void show_fps(std::string & name,
      std::string format,
      int64_t & latency,
      uint64_t & last_record_timestamp,
      uint64_t current_frame_timestamp,
      uint64_t & frame_count,
      int height,
      int width,
      int stride,
      int slice,
      int frame_id);

  std::unique_ptr<qrb_ros::transport::type::Image> convert_camera_frame_to_msg(
      std::unique_ptr<qrb_camera::CameraFrame> frame);

  void init();

  void start_camera();

  std::vector<std::shared_ptr<rclcpp::Publisher<IMAGE_TOPIC_TYPE>>> color_image_pub_vector_;
  std::vector<std::shared_ptr<rclcpp::Publisher<CAMERA_INFO_TOPIC_TYPE>>>
      color_camera_info_pub_vector_;

  std::shared_ptr<qrb_ros::camera::ConfigureParser> configure_;

  qrb_camera::CameraManager manager_;
  int camera_index_;

  int64_t time_offset_;

  std::vector<StreamTimeStamp> stream_ts_;

  std::vector<CAMERA_INFO_TOPIC_TYPE> camera_infos_;
};
}  // namespace camera
}  // namespace qrb_ros
#endif
