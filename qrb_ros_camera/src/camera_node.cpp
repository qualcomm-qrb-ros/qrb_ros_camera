/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

// Merged from the IQ9 and IQ10 qrb_ros_camera nodes (IQ10 2.0.3 as the base). Changes:
//   - no CameraType: the backend comes from plugin_discovery (`camera_backend` parameter,
//     empty = configured default) and the node never names one;
//   - the time offset uses the camera's own clock (CameraManager::device_now_ns()) instead
//     of CLOCK_BOOTTIME (IQ9) or QTimer (IQ10) chosen at build time, and is computed after
//     create_camera(), when the camera (and so its clock) is known;
//   - register_callback / start_camera failures throw, like the other construction steps.

#include "qrb_ros_camera/camera_node.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace qrb_ros
{
namespace camera
{
CameraNode::CameraNode(const rclcpp::NodeOptions & options) : rclcpp::Node("camera_node", options)
{
  RCLCPP_INFO(this->get_logger(), "QRB Camera Node start");

  if (!init()) {
    throw std::runtime_error("QRB Camera Node: configuration failed");
  }

  // A named backend replaces the configuration: only that one is loaded.
  const std::string & backend = configure_->get_camera_backend();
  std::vector<std::string> manual;
  if (!backend.empty()) {
    manual.push_back(backend);
  }
  manager_ = std::make_unique<qrb_camera::CameraManager>(manual);

  camera_index_ = manager_->create_camera(configure_->get_camera_id(), backend);
  if (camera_index_ < 0) {
    throw std::runtime_error(
        "QRB Camera Node: create_camera failed (see the CameraManager "
        "messages above; pluginlib-detect -d camera shows the configuration)");
  }

  auto & configure = configure_->get_camera_param();
  configure.camera_type = manager_->camera_backend(camera_index_);
  RCLCPP_INFO(this->get_logger(), "camera %d on backend '%s'", configure.camera_id,
      configure.camera_type.c_str());

  calculate_offset();
  // Keep following the camera clock: see time_offset_ in camera_node.hpp.
  offset_timer_ =
      this->create_wall_timer(std::chrono::seconds(1), [this]() { calculate_offset(); });

  if (!manager_->set_camera_parameter(camera_index_, configure)) {
    throw std::runtime_error("QRB Camera Node: set_camera_parameter failed");
  }

  using namespace std::placeholders;
  auto image_cb = std::bind(&CameraNode::publish_image, this, _1);
  if (!manager_->register_callback(camera_index_, image_cb)) {
    throw std::runtime_error("QRB Camera Node: register_callback failed");
  }

  if (!manager_->start_camera(camera_index_)) {
    throw std::runtime_error("QRB Camera Node: start_camera failed");
  }

  RCLCPP_INFO(this->get_logger(), "QRB Camera Node init success");
}

CameraNode::~CameraNode()
{
  if (offset_timer_) {
    offset_timer_->cancel();  // it calls into manager_
  }
  // Stops and destroys the camera (and unloads the backends) while the publishers still exist.
  manager_.reset();
}

bool CameraNode::init()
{
  configure_ = std::make_shared<ConfigureParser>(this);
  if (!configure_->init()) {
    RCLCPP_ERROR(this->get_logger(), "QRB Camera Node configure fail...");
    return false;
  }

  const auto & configure = configure_->get_camera_param();

  rclcpp::PublisherOptions pub_options;
  pub_options.use_intra_process_comm = rclcpp::IntraProcessSetting::Enable;

  stream_ts_.resize(configure.stream_size);
  camera_infos_.resize(configure.stream_size);

  std::string name = "cam" + std::to_string(configure.camera_id) + "_";

  for (int i = 0; i < configure.stream_size; i++) {
    auto img_publisher =
        this->create_publisher<IMAGE_TOPIC_TYPE>(name + configure.stream_names[i], 50, pub_options);

    auto info_publisher = this->create_publisher<CAMERA_INFO_TOPIC_TYPE>(
        name + configure.stream_names[i] + "_camera_info", 50, pub_options);

    color_image_pub_vector_.push_back(img_publisher);
    color_camera_info_pub_vector_.push_back(info_publisher);

    stream_ts_[i].stream_name = configure.stream_names[i];

    configure_->get_camera_info(
        configure.stream_widths[i], configure.stream_heights[i], camera_infos_[i]);
  }

  return true;
}

// offset = ROS now - camera clock now. The camera's clock is whatever its frame timestamps
// use (QMMF: CLOCK_BOOTTIME, QCarCam: QTimer); the backend reports it, the node does not
// need to know which one it is.
//
// Called once at start and then every second. The offset is not constant: time
// synchronization slews the system clock (tens of ppm; 33 ppm measured on IQ10, i.e.
// ~120 ms per hour against the QTimer) and may step it (first sync after boot). A change of
// more than 1 ms between two calls means the system clock was stepped: logged as a warning.
void CameraNode::calculate_offset()
{
  const int64_t device_time = manager_->device_now_ns(camera_index_);
  if (device_time < 0) {
    if (!time_offset_valid_) {
      RCLCPP_ERROR(this->get_logger(),
          "invalid camera clock time %ld, skip offset calculation (timestamps are not converted)",
          device_time);
    }
    return;  // keep the last valid offset
  }

  const int64_t cur_ros_time = (this->get_clock()->now()).nanoseconds();
  const int64_t offset = cur_ros_time - device_time;
  const int64_t previous = time_offset_.exchange(offset);

  if (!time_offset_valid_) {
    time_offset_valid_ = true;
    RCLCPP_INFO(this->get_logger(), "camera clock: %ld ros time: %ld time offset: %ld ns",
        device_time, cur_ros_time, offset);
  } else if (std::llabs(offset - previous) > 1000000) {
    RCLCPP_WARN(this->get_logger(),
        "time offset changed by %ld us (system clock stepped?): %ld -> %ld ns",
        (offset - previous) / 1000, previous, offset);
  } else {
    RCLCPP_DEBUG(
        this->get_logger(), "time offset %ld ns (drift %ld ns/s)", offset, offset - previous);
  }
}

void CameraNode::show_fps(const std::string & name,
    const std::string & format,
    int64_t & latency,
    uint64_t & last_record_timestamp,
    uint64_t current_frame_timestamp,
    int64_t & frame_count,
    int height,
    int width,
    int stride,
    int slice,
    int frame_id)
{
  int64_t cur_system_time = (this->get_clock()->now()).nanoseconds();

  if (frame_count == 0) {
    frame_count = 1;
    latency = cur_system_time - static_cast<int64_t>(current_frame_timestamp) - time_offset_.load();
    last_record_timestamp = current_frame_timestamp;
    return;
  }

  frame_count++;
  latency += cur_system_time - static_cast<int64_t>(current_frame_timestamp) - time_offset_.load();

  uint64_t diff = current_frame_timestamp - last_record_timestamp;

  uint64_t period = 5ULL * 1000000000ULL;

  if (diff > period) {
    double fps = (double)(frame_count * 1000000000) / (double)diff;

    int64_t average_latency = (latency / frame_count) / 1000;

    RCLCPP_INFO(this->get_logger(),
        "name: %s, fmt %s, w x h[%u x %u], stride x slice[%u x %u] "
        "frame_id %u, frameCnt %lu, "
        "Fps: %.4lf, ts %llu ns, latency %ld us",
        name.c_str(), format.c_str(), width, height, stride, slice, frame_id, frame_count, fps,
        current_frame_timestamp, average_latency);

    frame_count = 0;
  }
}

void CameraNode::publish_image(std::unique_ptr<qrb_camera::CameraFrame> frame)
{
  RCLCPP_DEBUG(
      this->get_logger(), "camera node publish image call: %s", frame->stream_name.c_str());

  int stream_index = -1;
  const auto & configure = configure_->get_camera_param();

  for (int i = 0; i < configure.stream_size; i++) {
    if (configure.stream_names[i] == frame->stream_name) {
      stream_index = i;
      break;
    }
  }

  if (stream_index == -1) {
    RCLCPP_ERROR(this->get_logger(), "unknown stream name: %s", frame->stream_name.c_str());
    return;
  }

  publish_color_image(stream_index, std::move(frame));
}

void CameraNode::publish_color_image(int stream_index,
    std::unique_ptr<qrb_camera::CameraFrame> frame)
{
  auto info = std::make_unique<CAMERA_INFO_TOPIC_TYPE>(camera_infos_[stream_index]);

  show_fps(frame->stream_name, frame->format, stream_ts_[stream_index].latency,
      stream_ts_[stream_index].timestamp, frame->timestamp, stream_ts_[stream_index].frame_count,
      frame->height, frame->width, frame->stride, frame->slice, frame->frame_id);

  auto msg = convert_camera_frame_to_msg(std::move(frame));

  info->header = msg->header;

  color_image_pub_vector_[stream_index]->publish(std::move(msg));
  color_camera_info_pub_vector_[stream_index]->publish(std::move(info));
}

std::unique_ptr<qrb_ros::transport::type::Image> CameraNode::convert_camera_frame_to_msg(
    std::unique_ptr<qrb_camera::CameraFrame> frame)
{
  auto image_handler = std::make_unique<qrb_ros::transport::type::Image>();

  int64_t raw_ts = frame->timestamp + time_offset_.load();
  if (raw_ts < 0) {
    RCLCPP_WARN_ONCE(this->get_logger(), "timestamp underflow, clamping to 0");
    raw_ts = 0;
  }
  auto ts = static_cast<uint64_t>(raw_ts);
  image_handler->header.stamp.sec = static_cast<int32_t>(ts / 1000000000ULL);
  image_handler->header.stamp.nanosec = static_cast<uint32_t>(ts % 1000000000ULL);

  image_handler->header.frame_id =
      "cam" + std::to_string(configure_->get_camera_param().camera_id) + "_" + frame->stream_name;

  image_handler->width = frame->width;
  image_handler->height = frame->height;

  image_handler->encoding = frame->format;

  auto dma_buf = std::make_shared<lib_mem_dmabuf::DmaBuffer>(frame->fd, frame->size);
  dma_buf->set_auto_release(false);

  image_handler->dmabuf = dma_buf;

  return image_handler;
}

}  // namespace camera
}  // namespace qrb_ros

// Register as rclcpp component, supports composition (component_container loading)
#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(qrb_ros::camera::CameraNode)
