/*
 *​​​​ Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include "qrb_ros_camera/camera_node.hpp"

#include <cstdint>
#include <stdexcept>

namespace qrb_ros
{
namespace camera
{
CameraNode::CameraNode(const rclcpp::NodeOptions & options)
  : rclcpp::Node("camera_node", options)
{
  RCLCPP_INFO(this->get_logger(), "QRB Camera Node start");

  camera_index_ = -1;

  if (!init()) {
    throw std::runtime_error("QRB Camera Node: configuration failed");
  }

  calculate_offset();

  // configure_->get_camera_id() from launch config

  camera_index_ = manager_.create_camera(
      qrb_camera::CameraType::QCarCam, configure_->get_camera_id());

  if (camera_index_ < 0) {
    throw std::runtime_error("QRB Camera Node: create_camera failed");
  }

  auto configure = configure_->get_camera_param();
  if (!manager_.set_camera_parameter(camera_index_, configure)) {
    throw std::runtime_error("QRB Camera Node: set_camera_parameter failed");
  }

  RCLCPP_INFO(this->get_logger(), "QRB Camera Node init success");

  start_camera();
}

void CameraNode::start_camera()
{
  using namespace std::placeholders;
  auto image_cb = std::bind(&CameraNode::publish_image, this, _1);

  if (!manager_.register_callback(camera_index_, image_cb)) {
    RCLCPP_ERROR(this->get_logger(), "QRB Camera Node: register_callback failed.");
    return;
  }

  if (!manager_.start_camera(camera_index_)) {
    RCLCPP_ERROR(this->get_logger(), "QRB Camera Node start failed.");
    return;
  }
}

CameraNode::~CameraNode()
{
  manager_.stop_camera(camera_index_);
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
    latency = cur_system_time - static_cast<int64_t>(current_frame_timestamp) - time_offset_;
    last_record_timestamp = current_frame_timestamp;
    return;
  }

  frame_count++;
  latency += cur_system_time - static_cast<int64_t>(current_frame_timestamp) - time_offset_;

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

void CameraNode::calculate_offset()
{
  uint64_t qtimer_time = getQtimerTime();

  if (qtimer_time > static_cast<uint64_t>(INT64_MAX)) {
    RCLCPP_ERROR(this->get_logger(),
        "invalid qtimer time %lu exceeds INT64_MAX, skip offset calculation", qtimer_time);
    return;
  }

  int64_t cur_system_time = static_cast<int64_t>(qtimer_time);

  int64_t cur_ros_time = (this->get_clock()->now()).nanoseconds();

  time_offset_ = cur_ros_time - cur_system_time;

  RCLCPP_INFO(this->get_logger(), "system time: %ld ros time: %ld time offset: %ld ns",
      cur_system_time, cur_ros_time, time_offset_);
}

void CameraNode::publish_image(std::unique_ptr<qrb_camera::CameraFrame> frame)
{
  RCLCPP_DEBUG(
      this->get_logger(), "camera node publish image call: %s", frame->stream_name.c_str());

  int stream_index = -1;
  auto format = frame->format;
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

  show_fps(frame->stream_name, "nv12", stream_ts_[stream_index].latency,
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

  int64_t raw_ts = frame->timestamp + time_offset_;
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

uint64_t CameraNode::getQtimerTime()
{
    uint64_t qTimerCount = 0, qTimerFreq = 0, qTimerSec = 0, qTimerNanosNSec = 0;
#if __aarch64__
    asm volatile("mrs %0, cntvct_el0" : "=r" (qTimerCount));
    asm volatile("mrs %0, cntfrq_el0" : "=r"(qTimerFreq));
#else
    asm volatile("mrrc p15, 1, %Q0, %R0, c14" : "=r" (qTimerCount));
    qTimerFreq =  19200000; //19.2 MHz TBD: find right asm instruction
#endif
    qTimerSec = (qTimerCount / qTimerFreq);
    qTimerNanosNSec = (qTimerCount % qTimerFreq);
    qTimerNanosNSec *= 1000000000;
    qTimerNanosNSec /= qTimerFreq;
    return (qTimerSec * 1000000000 + qTimerNanosNSec) ;
}

}  // namespace camera
}  // namespace qrb_ros

// Register as rclcpp component, supports composition (component_container loading)
#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(qrb_ros::camera::CameraNode)
