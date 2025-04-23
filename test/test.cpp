// Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Clear
#include "gtest/gtest.h"
#include "qrb_ros_camera/camera_ros2_node.hpp"

class NodeTestSuite : public ::testing::Test
{
protected:
  void SetUp() override { rclcpp::init(0, nullptr); }

  void TearDown() override { rclcpp::shutdown(); }
};

TEST_F(NodeTestSuite, RosMessageTest1)
{
  rclcpp::Node::SharedPtr test_node = std::make_shared<rclcpp::Node>("test_node");

  uint16_t h = 0;
  auto pub = test_node->create_publisher<IMAGE_TOPIC_TYPE>(IMAGE_TOPIC_NAME, 10);
  auto sub = test_node->create_subscription<sensor_msgs::msg::Image>(
      IMAGE_TOPIC_NAME, 10, [&h](const sensor_msgs::msg::Image & handler) { h = 1U; });

  EXPECT_EQ(pub->get_subscription_count(), 1U);
  EXPECT_EQ(sub->get_publisher_count(), 1U);

  auto message = sensor_msgs::msg::Image();

  pub->publish(message);
  std::this_thread::sleep_for(std::chrono::seconds(1));

  rclcpp::spin_some(test_node);

  EXPECT_EQ(h, 1U);

  pub.reset();
  sub.reset();
  test_node.reset();
}

TEST_F(NodeTestSuite, RosMessageTest2)
{
  rclcpp::Node::SharedPtr test_node = std::make_shared<rclcpp::Node>("test_node");

  uint16_t h = 0;
  auto pub = test_node->create_publisher<CAMERA_INFO_TOPIC_TYPE>(CAMERA_INFO_TOPIC_NAME, 10);
  auto sub = test_node->create_subscription<CAMERA_INFO_TOPIC_TYPE>(CAMERA_INFO_TOPIC_NAME, 10,
      [&h](const sensor_msgs::msg::CameraInfo::SharedPtr msg) { h = 1U; });

  EXPECT_EQ(pub->get_subscription_count(), 1U);
  EXPECT_EQ(sub->get_publisher_count(), 1U);

  auto message = std::make_shared<sensor_msgs::msg::CameraInfo>();
  pub->publish(*message);

  std::this_thread::sleep_for(std::chrono::seconds(1));
  rclcpp::spin_some(test_node);

  EXPECT_EQ(h, 1U);

  pub.reset();
  sub.reset();
  test_node.reset();
}