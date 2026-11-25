/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef QRB_ROS_CAMERA__CAMERA_NODE_HPP_
#define QRB_ROS_CAMERA__CAMERA_NODE_HPP_

#include <yaml-cpp/yaml.h>

#include <atomic>
#include <memory>
#include <sensor_msgs/msg/camera_info.hpp>
#include <string>
#include <vector>

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
  int64_t frame_count = 0;
  int64_t latency = 0;
};

// Publishes the frames of one camera. Knows no camera backend: CameraManager loads them
// through plugin_discovery and the `camera_backend` parameter (empty: the configured
// default) picks one. Any failure while constructing throws, so a component container
// reports it instead of keeping a node that never publishes.
class CameraNode : public rclcpp::Node
{
public:
  explicit CameraNode(const rclcpp::NodeOptions & options);
  ~CameraNode() override;

private:
  bool init();
  void calculate_offset();

  void publish_image(std::unique_ptr<qrb_camera::CameraFrame> frame);
  void publish_color_image(int stream_index, std::unique_ptr<qrb_camera::CameraFrame> frame);

  void show_fps(const std::string & name,
      const std::string & format,
      int64_t & latency,
      uint64_t & last_record_timestamp,
      uint64_t current_frame_timestamp,
      int64_t & frame_count,
      int height,
      int width,
      int stride,
      int slice,
      int frame_id);

  std::unique_ptr<qrb_ros::transport::type::Image> convert_camera_frame_to_msg(
      std::unique_ptr<qrb_camera::CameraFrame> frame);

  std::vector<std::shared_ptr<rclcpp::Publisher<IMAGE_TOPIC_TYPE>>> color_image_pub_vector_;
  std::vector<std::shared_ptr<rclcpp::Publisher<CAMERA_INFO_TOPIC_TYPE>>>
      color_camera_info_pub_vector_;

  std::shared_ptr<qrb_ros::camera::ConfigureParser> configure_;

  int camera_index_ = -1;
  // ROS time minus the camera's clock (ICameraDevice::now_ns()); added to frame timestamps.
  // Recomputed every second (offset_timer_): the two clocks drift apart when the system
  // clock is slewed or stepped by time synchronization (NTP / gPTP), while e.g. the QTimer
  // is a free-running hardware counter. Written by the timer, read by the frame callbacks.
  std::atomic<int64_t> time_offset_{0};
  bool time_offset_valid_ = false;
  rclcpp::TimerBase::SharedPtr offset_timer_;

  std::vector<StreamTimeStamp> stream_ts_;
  std::vector<CAMERA_INFO_TOPIC_TYPE> camera_infos_;

  // Created after the parameters are parsed (camera_backend decides what it loads).
  // Declared last, so it is destroyed first: the camera stops before the publishers it
  // calls back into go away.
  std::unique_ptr<qrb_camera::CameraManager> manager_;
};
}  // namespace camera
}  // namespace qrb_ros
#endif  // QRB_ROS_CAMERA__CAMERA_NODE_HPP_
