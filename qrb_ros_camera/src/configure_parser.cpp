/*
 *​​​​ Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include "qrb_ros_camera/configure_parser.hpp"

#include <yaml-cpp/yaml.h>

namespace qrb_ros
{
namespace camera
{
ConfigureParser::ConfigureParser(rclcpp::Node * node_handler) : node_handler_(node_handler) {}

bool ConfigureParser::init()
{
  int camera_id = node_handler_->declare_parameter("camera_id", -1);
  if (camera_id == -1) {
    RCLCPP_ERROR(node_handler_->get_logger(), "camera id is error param");
    init_ = false;
    return false;
  }

  int stream_size = node_handler_->declare_parameter("stream_size", 1);

  auto stream_name = node_handler_->declare_parameter("stream_name", std::vector<std::string>());

  if (stream_name.size() != stream_size) {
    RCLCPP_ERROR(node_handler_->get_logger(), "stream_name's size not equal stream_size");
    init_ = false;
    return false;
  }

  camera_.stream_names.reserve(stream_size);
  camera_.stream_formats.reserve(stream_size);
  camera_.stream_widths.reserve(stream_size);
  camera_.stream_heights.reserve(stream_size);
  camera_.stream_type.reserve(stream_size);

  for (int i = 0; i < stream_size; i++) {
    uint32_t height = node_handler_->declare_parameter(stream_name[i] + ".height", 1080);
    uint32_t width = node_handler_->declare_parameter(stream_name[i] + ".width", 1920);
    uint32_t fps = node_handler_->declare_parameter(stream_name[i] + ".fps", 30);
    std::string format =
        node_handler_->declare_parameter(stream_name[i] + ".format", std::string("nv12"));

    int bufferlist_id_raw = node_handler_->declare_parameter(stream_name[i] + ".bufferlist_id", 0);
    uint32_t bufferlist_id = static_cast<uint32_t>(bufferlist_id_raw);

    std::string stream_camera_info =
        node_handler_->declare_parameter(stream_name[i] + ".camera_info_path", std::string(""));

    camera_.stream_names.push_back(stream_name[i]);
    camera_.stream_formats.push_back(format);
    camera_.stream_widths.push_back(width);
    camera_.stream_heights.push_back(height);
    camera_.stream_fps.push_back(fps);
    camera_.stream_type.push_back(qrb_camera::StreamType::Track);
    camera_.stream_bufferlist_ids.push_back(bufferlist_id);

    qrb_camera::CameraIntrinsicParam param;
    if (!load_camera_info_param(stream_camera_info, param)) {
      RCLCPP_WARN(node_handler_->get_logger(),
          "no camera_info_path set for stream '%s', publishing empty intrinsics",
          stream_name[i].c_str());
    }
    camera_.camera_intrinsic_params.push_back(param);
  }

  camera_.camera_type = "qcarcam";
  camera_.camera_id = camera_id;
  camera_.stream_size = stream_size;

  camera_.isp_usecase = static_cast<uint32_t>(node_handler_->declare_parameter("isp_usecase", 0));
  camera_.input_mode = static_cast<uint32_t>(node_handler_->declare_parameter("input_mode", 0));

  init_ = true;
  return true;
}

qrb_camera::CameraConfigure & ConfigureParser::get_camera_param()
{
  return camera_;
}

bool ConfigureParser::get_camera_info(const uint32_t width,
    const uint32_t height,
    const int stream_index,
    sensor_msgs::msg::CameraInfo & camera_info)
{
  if (!init_) {
    RCLCPP_ERROR(node_handler_->get_logger(), "please call init at first");
    return false;
  }

  // per-stream camera info
  int idx = (stream_index >= 0 &&
                stream_index < static_cast<int>(camera_.camera_intrinsic_params.size())) ?
                stream_index :
                0;
  const auto & param = camera_.camera_intrinsic_params[idx];

  camera_info.width = width;
  camera_info.height = height;

  double scale_x = static_cast<double>(camera_info.width) / static_cast<double>(param.width);
  double scale_y = static_cast<double>(camera_info.height) / static_cast<double>(param.height);

  camera_info.distortion_model = param.distortion_model;

  for (size_t j = 0; j < param.distortion_params.size(); j++) {
    camera_info.d.push_back(param.distortion_params[j]);
  }

  camera_info.k[0] = param.camera_matrix[0] * scale_x;
  camera_info.k[2] = param.camera_matrix[2] * scale_x;
  camera_info.k[4] = param.camera_matrix[4] * scale_y;
  camera_info.k[5] = param.camera_matrix[5] * scale_y;
  camera_info.k[8] = 1.0;

  camera_info.p[0] = param.projection_matrix[0] * scale_x;
  camera_info.p[2] = param.projection_matrix[2] * scale_x;
  camera_info.p[5] = param.projection_matrix[5] * scale_y;
  camera_info.p[6] = param.projection_matrix[6] * scale_y;
  camera_info.p[10] = 1.0;

  camera_info.r[0] = 1.0;
  camera_info.r[4] = 1.0;
  camera_info.r[8] = 1.0;

  return true;
}

bool ConfigureParser::load_camera_info_param(std::string & path,
    qrb_camera::CameraIntrinsicParam & param)
{
  RCLCPP_INFO(node_handler_->get_logger(), "load camera intrinsic param");

  if (path.empty()) {
    RCLCPP_ERROR(node_handler_->get_logger(), "camera_info_path is empty");
    return false;
  }

  YAML::Node node;
  try {
    node = YAML::LoadFile(path);
  } catch (const YAML::Exception & e) {
    RCLCPP_ERROR(node_handler_->get_logger(), "failed to load camera info yaml '%s': %s",
        path.c_str(), e.what());
    return false;
  }

  if (node.IsNull()) {
    RCLCPP_ERROR(node_handler_->get_logger(), "load camera intrinsic param failed");
    return false;
  }

  param.height = node["image_height"].as<uint32_t>();
  param.width = node["image_width"].as<uint32_t>();

  param.distortion_model = node["distortion_model"].as<std::string>();

  const YAML::Node & distortion = node["distortion_coefficients"]["data"];
  if (distortion.IsSequence()) {
    for (std::size_t i = 0; i < distortion.size(); ++i) {
      param.distortion_params.push_back(distortion[i].as<double>());
    }
  }

  const YAML::Node & camera_matrix = node["camera_matrix"]["data"];
  if (camera_matrix.IsSequence() && camera_matrix.size() == 9) {
    for (std::size_t i = 0; i < 9; ++i) {
      param.camera_matrix[i] = camera_matrix[i].as<double>();
    }
  }

  const YAML::Node & projection_matrix = node["projection_matrix"]["data"];
  if (projection_matrix.IsSequence() && projection_matrix.size() == 12) {
    for (std::size_t i = 0; i < 12; ++i) {
      param.projection_matrix[i] = projection_matrix[i].as<double>();
    }
  }

  return true;
}

uint32_t ConfigureParser::get_camera_id()
{
  return camera_.camera_id;
}

}  // namespace camera
}  // namespace qrb_ros