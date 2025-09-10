// Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#include <sensor_msgs/msg/camera_info.hpp>

#include "qrb_ros_transport_image_type/image.hpp"
#include "rclcpp/rclcpp.hpp"

#define IMAGE_TOPIC_NAME "image"
#define CAMERA_INFO_TOPIC_NAME "camera_info"
#define IMAGE_TOPIC_TYPE qrb_ros::transport::type::Image
#define CAMERA_INFO_TOPIC_TYPE sensor_msgs::msg::CameraInfo
#define CAMERAROS2_WORKDIR "/var/tmp/ros2"

namespace qrb_ros::camera
{
class TestNode : public rclcpp ::Node
{
public:
  explicit TestNode(const rclcpp::NodeOptions & options);
  explicit TestNode(const std::string & name, const rclcpp::NodeOptions & options);
  int64_t cal_latency(int64_t sec, int64_t nsec);
  int64_t get_cur_time();
  void show_fps(std::string & name,
      std::string format,
      uint64_t current_frame_timestamp,
      int height,
      int width,
      int frame_id,
      int type = 0);

private:
  void handler_callback(const std::shared_ptr<const qrb_ros::transport::type::Image> & handler);
  void image_callback(const sensor_msgs::msg::Image::ConstSharedPtr & handler);

  void camera_info_callback(const sensor_msgs::msg::CameraInfo & camera_info);
  std::shared_ptr<rclcpp::Subscription<qrb_ros::transport::type::Image>> handler_sub_;
  std::shared_ptr<rclcpp::Subscription<sensor_msgs::msg::Image>> image_sub_;
  std::shared_ptr<rclcpp::Subscription<sensor_msgs::msg::CameraInfo>> camera_info_sub_;
  std::shared_ptr<sensor_msgs::msg::CameraInfo> m_p_camera_info_;
  int64_t g_receive_last_fpstime[2];   // only add this for test fps
  int64_t g_receive_total_latency[2];  // only add this for test fps
  uint64_t g_receive_frame_count[2];   // only add this for test fps
  bool dump_ = false;
  bool dump_camera_info_ = false;
  bool enable_depth_ = false;
  int handler_frame_id_ = 0;
  int depth_frame_id_ = 0;
};

// Constructor must be with parameter NodeOptions (component 's requirement)
TestNode::TestNode(const rclcpp::NodeOptions & options) : TestNode("test_node", options) {}

TestNode::TestNode(const std::string & name, const rclcpp::NodeOptions & options)
  : rclcpp::Node(name, options)
{
  enable_depth_ = this->declare_parameter("subscirbe_depth", false);
  dump_ = this->declare_parameter("dump", false);
  dump_camera_info_ = this->declare_parameter("dump_camera_info", false);
  rclcpp::SubscriptionOptions sub_options;
  sub_options.use_intra_process_comm = rclcpp::IntraProcessSetting::Enable;
  g_receive_frame_count[0] = 0;
  g_receive_frame_count[1] = 0;
  // create image type adaption 's subscriber
  handler_sub_ = this->create_subscription<IMAGE_TOPIC_TYPE>(IMAGE_TOPIC_NAME, 10,
      std::bind(&TestNode::handler_callback, this, std::placeholders::_1), sub_options);
  camera_info_sub_ = this->create_subscription<CAMERA_INFO_TOPIC_TYPE>(CAMERA_INFO_TOPIC_NAME, 10,
      std::bind(&TestNode::camera_info_callback, this, std::placeholders::_1), sub_options);
  if (enable_depth_) {
    image_sub_ = this->create_subscription<sensor_msgs::msg::Image>("depth_image", 10,
        std::bind(&TestNode::image_callback, this, std::placeholders::_1), sub_options);
  }
}

int64_t TestNode::get_cur_time()
{
  int64_t cur_time = (this->get_clock()->now()).nanoseconds();
  return cur_time;
}

int64_t TestNode::cal_latency(int64_t sec, int64_t nsec)
{
  int64_t curtime = TestNode::get_cur_time();
  int64_t ts = 1000000000ULL * sec + nsec;
  int64_t diff = curtime - ts;
  return diff;
}

void TestNode::show_fps(std::string & name,
    std::string format,
    uint64_t current_frame_timestamp,
    int height,
    int width,
    int frame_id,
    int type)
{
  uint64_t cur_system_time = get_clock()->now().nanoseconds();
  if (g_receive_frame_count[type] == 0) {
    g_receive_frame_count[type] = 1;
    g_receive_total_latency[type] = cur_system_time - current_frame_timestamp;
    g_receive_last_fpstime[type] = cur_system_time;
    return;
  }
  g_receive_frame_count[type]++;
  g_receive_total_latency[type] += cur_system_time - current_frame_timestamp;
  uint64_t diff = cur_system_time - g_receive_last_fpstime[type];
  uint64_t period = 5ULL * 1000000000ULL;
  if (diff > period) {
    double fps = (double)(g_receive_frame_count[type] * 1000000000) / (double)diff;
    uint64_t average_latency = g_receive_total_latency[type] / g_receive_frame_count[type] / 1000;
    RCLCPP_INFO(this->get_logger(),
        "camera %s, fmt %s, w x h[%u x %u], frame_id %u, frameCnt %u, "
        "Fps: %.4lf, ts %lld ns, latency %ld us \n",
        name.c_str(), format.c_str(), width, height, frame_id, g_receive_frame_count[type], fps,
        current_frame_timestamp, average_latency);
    g_receive_frame_count[type] = 0;
  }
}

void TestNode::image_callback(const sensor_msgs::msg::Image::ConstSharedPtr & handler)
{
  RCLCPP_DEBUG(this->get_logger(),
      "receive imageHandler: width=%d,height=%d frame_id: %s encoding: %s", handler->width,
      handler->height, handler->header.frame_id.c_str(), handler->encoding.c_str());

  this->get_parameter("dump", dump_);
  std::string name;
  depth_frame_id_++;
  uint64_t timestamp = handler->header.stamp.sec * 1000000000ULL + handler->header.stamp.nanosec;

  show_fps(name, handler->encoding, timestamp, handler->height, handler->width, depth_frame_id_, 1);
  if (dump_) {
    int size = handler->height * handler->step;
    auto bufData = &handler->data[0];
    char fname[256];
    const char * prefix_name = "test";
    std::string node_name = this->get_name();
    snprintf(fname, sizeof(fname), "%s/%s-%s_wh[%dx%d]_%s-%d-%s.raw", CAMERAROS2_WORKDIR,
        prefix_name, node_name.c_str(), handler->width, handler->height, name.c_str(),
        depth_frame_id_, handler->encoding.c_str());
    FILE * fd = fopen(fname, "wb");
    if (fd == NULL) {
      RCLCPP_ERROR(this->get_logger(),
          "can't open this file: %s, please check whether diectory exist!", fname);
      return;
    }
    fwrite(bufData, size, 1, fd);
    fclose(fd);
    fd = NULL;
  }
}

void TestNode::handler_callback(
    const std::shared_ptr<const qrb_ros::transport::type::Image> & handler)
{
  RCLCPP_DEBUG(this->get_logger(),
      "receive imageHandler: width=%d,height=%d frame_id: %s encoding: %s", handler->width,
      handler->height, handler->header.frame_id.c_str(), handler->encoding.c_str());

  this->get_parameter("dump", dump_);
  std::string name;
  handler_frame_id_++;
  uint64_t timestamp = handler->header.stamp.sec * 1000000000ULL + handler->header.stamp.nanosec;

  show_fps(name, handler->encoding, timestamp, handler->height, handler->width, handler_frame_id_);
  if (dump_) {
    std::shared_ptr<lib_mem_dmabuf::DmaBuffer> buffer = handler->dmabuf;
    buffer->map();
    buffer->sync_start();
    int size = buffer->size();
    uint8_t * bufData = (uint8_t *)buffer->addr();
    char fname[256];
    const char * prefix_name = "test";
    std::string node_name = this->get_name();
    snprintf(fname, sizeof(fname), "%s/%s-%s_wh[%dx%d]_%s-%d.%s", CAMERAROS2_WORKDIR, prefix_name,
        node_name.c_str(), handler->width, handler->height, name.c_str(), handler_frame_id_,
        handler->encoding.c_str());
    FILE * fd = fopen(fname, "wb");
    if (fd == NULL) {
      RCLCPP_ERROR(this->get_logger(),
          "can't open this file: %s, please check whether diectory exist!", fname);
      return;
    }
    fwrite(bufData, size, 1, fd);
    fclose(fd);
    fd = NULL;
    buffer->unmap();
    buffer->sync_end();
  }
}

void TestNode::camera_info_callback(const sensor_msgs::msg::CameraInfo & camera_info)
{
  RCLCPP_DEBUG(this->get_logger(), "Received CameraInfo:");
  this->get_parameter("dump_camera_info", dump_camera_info_);
  if (dump_camera_info_) {
    RCLCPP_INFO(this->get_logger(), "distortion_coefficients:");
    for (int i = 0; i < camera_info.d.size(); ++i) {
      RCLCPP_INFO(this->get_logger(), "%f", camera_info.d[i]);
    }

    RCLCPP_INFO(this->get_logger(), "camera_matrix:");
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        RCLCPP_INFO(this->get_logger(), "%f", camera_info.k[i * 3 + j]);
      }
    }

    RCLCPP_INFO(this->get_logger(), "Projection Matrix:");
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 4; ++j) {
        RCLCPP_INFO(this->get_logger(), "%f", camera_info.p[i * 4 + j]);
      }
    }

    RCLCPP_INFO(this->get_logger(), "rectification_matrix:");
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        RCLCPP_INFO(this->get_logger(), "%f", camera_info.r[i * 3 + j]);
      }
    }
  }
}
}  // namespace qrb_ros::camera

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(qrb_ros::camera::TestNode)
