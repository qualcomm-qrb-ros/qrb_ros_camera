/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include "qrb_camera/camera_manager.hpp"

#include <iostream>

namespace qrb_camera
{
CameraManager::~CameraManager()
{
  std::lock_guard<std::mutex> lock(mtx_);
  if (!devices_.empty()) {
    for (int i = 0; i < devices_.size(); i++) {
      if (devices_[i] != nullptr) {
        destroy_camera(i);
      }
    }
  }
}

int CameraManager::create_camera(CameraType type, uint32_t camera_id)
{
  std::lock_guard<std::mutex> lock(mtx_);
  for (int i = 0; i < devices_.size(); i++) {
    if (devices_[i]->get_type() == type && devices_[i]->get_id() == camera_id) {
      std::cout << "[ERROR] [" << logger_ << "]: please destroy the same camera id at first."
                << std::endl;
      return -1;
    }
  }

  if (type == CameraType::QMMF) {
    ICameraDevice * device = new QMMFCamera(camera_id);
    devices_.push_back(device);

    std::shared_ptr<CameraConfigure> param = nullptr;
    params_.push_back(param);

    std::vector<ICameraListener *> listener;
    listeners_.push_back(listener);
    return devices_.size() - 1;
  } else {
    std::cout << "[INFO] [" << logger_ << "]: create camera failed." << std::endl;
  }
  return -1;
}

bool CameraManager::destroy_camera(int index)
{
  if (index >= devices_.size()) {
    std::cout << "[WARNING] [" << logger_ << "]: please create the camera at first." << std::endl;
    return false;
  }
  if (devices_[index] == nullptr) {
    std::cout << "[WARNING] [" << logger_ << "]: camera already destroy." << std::endl;
    return false;
  }
  if (devices_[index]->working()) {
    stop_camera(index);
  }
  delete devices_[index];
  devices_[index] = nullptr;
  return true;
}

bool CameraManager::set_camera_parameter(int index, CameraConfigure & param)
{
  if (index >= devices_.size()) {
    std::cout << "[WARNING] [" << logger_ << "]: please create the camera at first." << std::endl;
    return false;
  }
  params_[index] = std::make_shared<CameraConfigure>(param);
  return true;
}

void CameraManager::convert_detail_to_param(int index, std::vector<CameraParam> & param)
{
  auto it = params_.begin() + index;
  CameraParam camera_pam;
  auto size = (*it)->stream_size;
  for (int j = 0; j < size; j++) {
    if ((*it)->stream_type[j] == StreamType::Track) {
      StreamParam stream_pam;
      stream_pam.width = (*it)->stream_widths[j];
      stream_pam.height = (*it)->stream_heights[j];
      stream_pam.frame_rate = (*it)->stream_fps[j];
      stream_pam.format = (*it)->stream_formats[j];
      stream_pam.name = (*it)->stream_names[j];
      camera_pam.track_params.push_back(stream_pam);
    } else if ((*it)->stream_type[j] == StreamType::Snapshot) {
      StreamParam stream_pam;
      stream_pam.width = (*it)->stream_widths[j];
      stream_pam.height = (*it)->stream_heights[j];
      stream_pam.frame_rate = (*it)->stream_fps[j];
      stream_pam.format = (*it)->stream_formats[j];
      stream_pam.name = (*it)->stream_names[j];
      camera_pam.snapshot_params.push_back(stream_pam);
    }
  }
  param.push_back(camera_pam);
}

bool CameraManager::start_camera(int index)
{
  if (index >= devices_.size()) {
    std::cout << "[WARNING] [" << logger_ << "]: please create the camera at first." << std::endl;
    return false;
  }
  if (params_[index] == nullptr) {
    std::cout << "[WARNING] [" << logger_ << "]: please set camera parameter before start camera."
              << std::endl;
    return false;
  }
  std::vector<CameraParam> param;
  convert_detail_to_param(index, param);
  devices_[index]->set_param(param);
  devices_[index]->init();
  int ret = devices_[index]->start_camera();
  if (ret != 0) {
    std::cout << "[ERROR] [" << logger_ << "]: start camera failed, please check the output."
              << std::endl;
    return false;
  }
  std::cout << "[INFO] [" << logger_ << "]: start camera success!!" << std::endl;
  return true;
}
void CameraManager::stop_camera(int index)
{
  if (index >= devices_.size()) {
    std::cout << "[WARNING] [" << logger_ << "]: please create the camera at first." << std::endl;
    return;
  }
  if (!devices_[index]->working()) {
    return;
  }
  for (auto listener : listeners_[index]) {
    devices_[index]->remove_listener(listener);
    delete listener;
  }
  listeners_[index].clear();
  devices_[index]->stop_camera();
}

bool CameraManager::register_callback(int index, ImageCallback image_cb)
{
  if (index >= devices_.size()) {
    std::cout << "[WARNING] [" << logger_ << "]: please create the camera at first." << std::endl;
    return false;
  }
  if (devices_[index] == nullptr) {
    std::cout << "[WARNING] [" << logger_ << "]: the camera is not work now." << std::endl;
    return false;
  }
  auto type = devices_[index]->get_type();
  if (type == CameraType::QMMF) {
    std::cout << "[INFO] [" << logger_ << "]: create qmmf listener!!" << std::endl;
    ICameraListener * listener = new QMMFListener(image_cb);
    listeners_[index].push_back(listener);
    devices_[index]->add_listener(listener);
    return true;
  }
  return false;
}
}  // namespace qrb_camera
