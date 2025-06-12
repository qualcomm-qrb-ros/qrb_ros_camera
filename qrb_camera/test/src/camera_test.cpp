/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

#include "qrb_camera/camera_manager.hpp"
#include "qrb_camera/qrb_camera_common.hpp"

using namespace qrb_camera;
int main()
{
  CameraManager manager;
  auto index = manager.create_camera(qrb_camera::CameraType::QMMF, 0);
  CameraConfigure detail;
  detail.camera_id = 0;
  detail.camera_type = "single";
  detail.stream_size = 1;

  detail.stream_names.push_back("main");
  detail.stream_formats.push_back("nv12");
  detail.stream_widths.push_back(1280);
  detail.stream_heights.push_back(720);
  detail.stream_fps.push_back(30);

  manager.set_camera_parameter(index, detail);

  if (!manager.start_camera(index)) {
    std::cout << "start camera failed !!! " << std::endl;
    return -1;
  }
  auto image_cb = [&](std::unique_ptr<CameraFrame> frame) {
    std::cout << "frame name: " << frame->stream_name << " frame id: " << frame->frame_id
              << " width: " << frame->width << " height: " << frame->height
              << " format: " << frame->format << " timestamp: " << frame->timestamp
              << " fd: " << frame->fd << std::endl;
  };

  manager.register_callback(index, image_cb);
  std::this_thread::sleep_for(std::chrono::seconds(10));
  return 0;
}