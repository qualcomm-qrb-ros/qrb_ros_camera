// Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#include "qrb_ros_camera/camera_ros2_node.hpp"

#include "qrb_ros_camera/camera_ros2_common.hpp"

namespace qrb_ros::camera
{
CameraNode::CameraNode(const rclcpp::NodeOptions & options) : rclcpp::Node("CameraNode", options)
{
  RCLCPP_INFO(this->get_logger(), "Camera Ros2 Node statrt initialization...");
  m_p_config_ = NULL;
  handler_pub_ = NULL;
  m_p_camera_info_ = std::make_shared<sensor_msgs::msg::CameraInfo>();
  // create topic publisher with type QrbRosImageTypeAdapter
  rclcpp::PublisherOptions pub_options;
  pub_options.use_intra_process_comm = rclcpp::IntraProcessSetting::Enable;
  handler_pub_ = this->create_publisher<IMAGE_TOPIC_TYPE>(IMAGE_TOPIC_NAME, 50, pub_options);
  camera_info_pub_ =
      this->create_publisher<CAMERA_INFO_TOPIC_TYPE>(CAMERA_INFO_TOPIC_NAME, 10, pub_options);
  // load parameters in launch file
  load_config();
  load_camerainfo();
  std::function<void(CameraRos2Frame * frame)> publish_image = [&](CameraRos2Frame * frame) {
    CameraNode::publish_image(frame);
  };
  m_publish_frame_function_ = publish_image;
  if (!this->init()) {
    RCLCPP_ERROR(this->get_logger(), "Camera Ros2 Node run fail...");
  }
}

CameraNode::~CameraNode()
{
  RCLCPP_INFO(this->get_logger(), "Camera Ros2 Node stopping");
  m_publish_frame_function_ = NULL;
  m_p_pipeline_->destroy();
  m_p_config_ = NULL;
}

bool CameraNode::init()
{
  m_p_pipeline_ = QmmfRos2Pipeline::create_instance(m_p_config_);
  if (m_p_pipeline_ == nullptr) {
    RCLCPP_ERROR(this->get_logger(), "qmmf pipeline create failed");
    return false;
  }
  m_p_pipeline_->register_publish(m_publish_frame_function_);
  this->SyncTimestamp();
  return true;
}

void CameraNode::load_config()
{
  RCLCPP_INFO(this->get_logger(), "load camera parameters...");
  CameraRos2CamConfig cam_cfg;
  cam_cfg.pipeline_id = this->declare_parameter("pipelinId", 0);
  cam_cfg.camera_id = this->declare_parameter("cameraId", 0);
  cam_cfg.width = this->declare_parameter("width", 1920);
  cam_cfg.height = this->declare_parameter("height", 1080);
  cam_cfg.format = this->declare_parameter("format", "nv12");
  cam_cfg.fps = this->declare_parameter("fps", 30);
  cam_cfg.publish_freq = this->declare_parameter("publish_freq", 1);
  cam_cfg.latency_type = this->declare_parameter("publish_latency_type", 1);
  get_alignment(cam_cfg.format, cam_cfg.width, cam_cfg.height, &cam_cfg.stride, &cam_cfg.slice);
  // get camera config
  m_p_config_ = CameraRos2Config::create_instance(&cam_cfg);
  if (m_p_config_ == nullptr) {
    RCLCPP_ERROR(this->get_logger(), "create camera parameters config fail...");
    return;
  }
  m_p_config_->print_config();
}

void CameraNode::convert_to_camerainfo(const YAML::Node & yaml_node)
{
  RCLCPP_INFO(this->get_logger(), "load camera info yaml file...");
  // get camerainfo from yaml
  m_p_camera_info_->height = yaml_node["image_height"].as<uint32_t>();
  m_p_camera_info_->width = yaml_node["image_width"].as<uint32_t>();
  m_p_camera_info_->distortion_model = yaml_node["distortion_model"].as<std::string>();

  // get distortion_coeffs
  const YAML::Node & distortion_coeffs_node = yaml_node["distortion_coefficients"]["data"];
  if (distortion_coeffs_node.IsSequence()) {
    for (std::size_t i = 0; i < distortion_coeffs_node.size(); ++i) {
      m_p_camera_info_->d.push_back(distortion_coeffs_node[i].as<double>());
    }
  }

  // get camera_matrix
  const YAML::Node & camera_matrix_node = yaml_node["camera_matrix"]["data"];
  if (camera_matrix_node.IsSequence() && camera_matrix_node.size() == 9) {
    for (std::size_t i = 0; i < 9; ++i) {
      m_p_camera_info_->k[i] = camera_matrix_node[i].as<double>();
    }
  }

  // get projection_matrix
  const YAML::Node & projection_matrix_node = yaml_node["projection_matrix"]["data"];
  if (projection_matrix_node.IsSequence() && projection_matrix_node.size() == 12) {
    for (std::size_t i = 0; i < 12; ++i) {
      m_p_camera_info_->p[i] = projection_matrix_node[i].as<double>();
    }
  }

  // get rectification_matrix
  const YAML::Node & rectification_matrix_node = yaml_node["rectification_matrix"]["data"];
  if (rectification_matrix_node.IsSequence() && rectification_matrix_node.size() == 9) {
    for (std::size_t i = 0; i < 9; ++i) {
      m_p_camera_info_->r[i] = rectification_matrix_node[i].as<double>();
    }
  }
}

void CameraNode::load_camerainfo()
{
  RCLCPP_INFO(this->get_logger(), "enter CameraNode::load_camerainfo");
  camera_info_url_ = this->declare_parameter("camera_info_path", "");
  YAML::Node yaml_node = YAML::LoadFile(camera_info_url_);
  if (yaml_node.IsNull()) {
    RCLCPP_ERROR(this->get_logger(), "load camera info yaml file fail");
    return;
  }
  convert_to_camerainfo(yaml_node);
}

int64_t CameraNode::get_cur_time()
{
  struct timespec spec;
  clock_gettime(CLOCK_BOOTTIME, &spec);
  int64_t cur_time = 1000000000 * spec.tv_sec + spec.tv_nsec;
  return cur_time;
}

int64_t CameraNode::get_cur_ros_time()
{
  int64_t cur_time = (this->get_clock()->now()).nanoseconds();
  return cur_time;
}

int64_t CameraNode::cal_latency(int64_t ts)
{
  int64_t curtime = CameraNode::get_cur_ros_time();
  int64_t diff = curtime - ts;
  return diff;
}

void CameraNode::SyncTimestamp()
{
  int64_t cur_ros_time_ = this->get_cur_ros_time();
  int64_t cur_qmmf_time_ = this->get_cur_time();
  time_offset_ = cur_ros_time_ - cur_qmmf_time_;
}

void CameraNode::show_fps(CameraRos2Frame * frame)
{
  uint32_t pipeline_id = frame->info.pipeline_id;
  struct timespec time;
  time.tv_sec = time.tv_nsec = 0;
  clock_gettime(CLOCK_MONOTONIC, &time);
  int64_t now = S2NS(time.tv_sec) + time.tv_nsec;
  if (frame->info.frame_id == 0 || g_frame_count == 0) {
    /// reset
    g_last_fpstime = now;
    g_total_latency = 0;
    g_frame_count = 0;
  }
  g_frame_count++;
  volatile int64_t diff = now - g_last_fpstime;
  g_total_latency += frame->info.latency;
  if (diff > S2NS(5)) {
    double fps = (((double)(g_frame_count)) * (double)(S2NS(1))) / (double)diff;
    int64_t latency = g_total_latency / g_frame_count / 1000;
    RCLCPP_INFO(rclcpp::get_logger("qrb_ros_camera"),
        "camera Pipeline %u, Camera %d, fmt %s, w x h[%u x %u], str x sli[%u x %u], "
        "frame_id %u, frameCnt %u, Fps: %.4lf, ts %lld ns, latency %ld us \n",
        frame->info.pipeline_id, frame->info.camera_id, frame->info.format.c_str(),
        frame->info.width, frame->info.height, frame->info.stride, frame->info.slice,
        frame->info.frame_id, g_frame_count, fps, frame->info.timestamp, latency);
    g_last_fpstime = now;
    g_total_latency = 0;
    g_frame_count = 0;
  }
}

void CameraNode::get_image_info(CameraRos2Frame * frame,
    std::unique_ptr<qrb_ros::transport::type::Image> & img_msg)
{
  img_msg->header.stamp.sec = (int32_t)(frame->info.timestamp / 1000000000);
  img_msg->header.stamp.nanosec = (uint32_t)(frame->info.timestamp % 1000000000);
  char frame_id_str[48];
  snprintf((char *)frame_id_str, sizeof(frame_id_str), CAMERA_ROS2_FRAME_ID,
      frame->info.pipeline_id, frame->info.camera_id, frame->info.frame_id);
  img_msg->header.frame_id = (const char *)frame_id_str;
  img_msg->width = frame->info.width;
  img_msg->height = frame->info.height;
  img_msg->encoding = frame->info.format;
}

void CameraNode::publish_image(CameraRos2Frame * frame)
{
  if (frame == nullptr) {
    RCLCPP_ERROR(this->get_logger(), "camera publish frame is nullptr...");
  }
  frame->info.timestamp = frame->info.timestamp + time_offset_;
  if (frame->info.latency_type == CameraRos2LatencyDistribution) {
    int64_t latency = CameraNode::cal_latency(frame->info.timestamp);
    RCLCPP_DEBUG(this->get_logger(), "camera publish latency is %ld(us)", (latency / 1000));
    frame->info.timestamp = CameraNode::get_cur_ros_time();
    frame->info.latency = latency;
  } else {
    frame->info.timestamp = frame->info.timestamp;
    frame->info.latency = CameraNode::cal_latency(frame->info.timestamp);
    RCLCPP_DEBUG(
        this->get_logger(), "camera publish latency is %ld(us)", (frame->info.latency / 1000));
  }
  m_p_camera_info_->header.stamp.sec = (int32_t)(frame->info.timestamp / 1000000000);
  m_p_camera_info_->header.stamp.nanosec = (uint32_t)(frame->info.timestamp % 1000000000);
  camera_info_pub_->publish(*m_p_camera_info_);
  this->show_fps(frame);
  auto image_handler = std::make_unique<qrb_ros::transport::type::Image>();
  get_image_info(frame, image_handler);
  auto dma_buf = std::make_shared<lib_mem_dmabuf::DmaBuffer>(frame->info.fd, frame->info.size);
  dma_buf->set_auto_release(false);
  image_handler->dmabuf = dma_buf;
  handler_pub_->publish(std::move(image_handler));
}
}  // namespace qrb_ros::camera

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(qrb_ros::camera::CameraNode)
