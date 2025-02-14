// Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#include "qrb_ros_camera/test_ros2_node.hpp"

#include "rclcpp/subscription_options.hpp"

namespace qrb_ros::camera
{
// Constructor must be with parameter NodeOptions (component 's requirement)
TestNode::TestNode(const rclcpp::NodeOptions & options) : TestNode("test_node", options) {}

TestNode::TestNode(const std::string & name, const rclcpp::NodeOptions & options)
  : rclcpp::Node(name, options)
{
  m_p_frame_ = std::make_unique<CameraRos2Frame>();
  dump_ = this->declare_parameter("dump", false);
  dump_camera_info_ = this->declare_parameter("dump_camera_info", false);
  rclcpp::SubscriptionOptions sub_options;
  sub_options.use_intra_process_comm = rclcpp::IntraProcessSetting::Enable;

  // create image type adaption 's subscriber
  handler_sub_ = this->create_subscription<IMAGE_TOPIC_TYPE>(IMAGE_TOPIC_NAME, 10,
      std::bind(&TestNode::handler_callback, this, std::placeholders::_1), sub_options);
  camera_info_sub_ = this->create_subscription<CAMERA_INFO_TOPIC_TYPE>(CAMERA_INFO_TOPIC_NAME, 10,
      std::bind(&TestNode::camera_info_callback, this, std::placeholders::_1), sub_options);
}

int64_t TestNode::get_cur_time()
{
  int64_t cur_time = (this->get_clock()->now()).nanoseconds();
  return cur_time;
}

int64_t TestNode::cal_latency(int64_t sec, int64_t nsec)
{
  int64_t curtime = TestNode::get_cur_time();
  int64_t ts = 1000000000 * sec + nsec;
  int64_t diff = curtime - ts;
  return diff;
}

void TestNode::show_receive_fps(CameraRos2Frame * frame)
{
  uint32_t receive_pipeline_id = frame->info.pipeline_id;
  struct timespec time;
  time.tv_sec = time.tv_nsec = 0;
  clock_gettime(CLOCK_MONOTONIC, &time);
  int64_t now = S2NS(time.tv_sec) + time.tv_nsec;
  if (frame->info.frame_id == 0 || g_receive_frame_count == 0) {
    /// reset
    g_receive_last_fpstime = now;
    g_receive_total_latency = 0;
    g_receive_frame_count = 0;
  }
  g_receive_frame_count++;
  volatile int64_t receive_diff = now - g_receive_last_fpstime;
  g_receive_total_latency += frame->info.latency;
  if (receive_diff > S2NS(5)) {
    double fps = (((double)(g_receive_frame_count)) * (double)(S2NS(1))) / (double)receive_diff;
    int64_t latency = g_receive_total_latency / g_receive_frame_count / 1000;
    RCLCPP_INFO(this->get_logger(),
        "test Pipeline %u, Camera %d, fmt %s, w x h[%u x %u],  "
        "frame_id %u, frameCnt %u, Fps: %.4lf, ts %lld ns, latency %ld us \n",
        frame->info.pipeline_id, frame->info.camera_id, frame->info.format.c_str(),
        frame->info.width, frame->info.height, frame->info.frame_id, g_receive_frame_count, fps,
        frame->info.timestamp, latency);
    g_receive_last_fpstime = now;
    g_receive_total_latency = 0;
    g_receive_frame_count = 0;
  }
}

void TestNode::handler_callback(const qrb_ros::transport::type::Image & handler)
{
  RCLCPP_DEBUG(this->get_logger(), "receive imageHandler: width='%d',height='%d' \taddr = 0x%lx",
      handler.width, handler.height, reinterpret_cast<std::uintptr_t>(&handler));
  int64_t latency = this->cal_latency(handler.header.stamp.sec, handler.header.stamp.nanosec);
  RCLCPP_DEBUG(this->get_logger(), "camera receive latency is %ld(us)", (latency / 1000));
  this->get_parameter("dump", dump_);
  m_p_frame_->info.latency = latency;
  m_p_frame_->info.width = handler.width;
  m_p_frame_->info.height = handler.height;
  m_p_frame_->info.format = handler.encoding;
  sscanf(handler.header.frame_id.c_str(), "cam[%d_%d] id[%d]", &m_p_frame_->info.pipeline_id,
      &m_p_frame_->info.camera_id, &m_p_frame_->info.frame_id);
  this->show_receive_fps(m_p_frame_.get());
  if (dump_) {
    std::shared_ptr<lib_mem_dmabuf::DmaBuffer> buffer = handler.dmabuf;
    buffer->map();
    buffer->sync_start();
    m_p_frame_->data = (uint8_t *)buffer->addr();
    int size = buffer->size();
    uint8_t * bufData = m_p_frame_->data;
    char fname[256];
    const char * name = "test";
    snprintf(fname, sizeof(fname), "%s/%s_wh[%dx%d]_id[%d-%d-%d].%s", CAMERAROS2_WORKDIR, name,
        m_p_frame_->info.width, m_p_frame_->info.height, m_p_frame_->info.pipeline_id,
        m_p_frame_->info.camera_id, m_p_frame_->info.frame_id,
        get_file_type(get_format_type(m_p_frame_->info.format)));
    FILE * fd = fopen(fname, "wb");
    fwrite(bufData, size, 1, fd);
    fclose(fd);
    fd = NULL;
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
