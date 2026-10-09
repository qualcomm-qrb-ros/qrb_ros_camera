/*
 *​​​​ Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include "qrb_camera/camera_manager.hpp"

#include <iostream>

#include "qrb_camera/qcarcam_camera.hpp"
#include "qrb_camera/qcarcam_listener.hpp"


namespace qrb_camera
{

CameraManager::~CameraManager()
{
  std::lock_guard<std::mutex> lock(mtx_);
  for (size_t i = 0; i < devices_.size(); i++) {
    if (devices_[i] != nullptr) {
      destroy_camera(static_cast<int>(i));
    }
  }
}

int CameraManager::create_camera(CameraType type, uint32_t camera_id)
{
  std::lock_guard<std::mutex> lock(mtx_);

  std::cout << "devices_.size(): " << devices_.size() << std::endl;

  for (size_t i = 0; i < devices_.size(); i++) {
    if (devices_[i] != nullptr &&
        devices_[i]->get_type() == type &&
        devices_[i]->get_id() == camera_id) {
      std::cout << "[ERROR] [" << logger_ << "]: please destroy the same camera id at first."
                << std::endl;
      return -1;
    }
  }

  ICameraDevice * device = nullptr;
  if (type == CameraType::QCarCam) {
    device = new QCarCamCamera(camera_id);
  }

  if (device == nullptr) {
    std::cout << "[ERROR] [" << logger_ << "]: unsupported camera type for this build." << std::endl;
    return -1;
  }

  devices_.push_back(device);
  params_.push_back(nullptr);
  listeners_.push_back(std::vector<ICameraListener *>());

  return static_cast<int>(devices_.size() - 1);
}

bool CameraManager::destroy_camera(int index)
{
  if (index < 0 || static_cast<size_t>(index) >= devices_.size()) {
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
  if (index < 0 || static_cast<size_t>(index) >= devices_.size()) {
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

  camera_pam.isp_usecase = (*it)->isp_usecase;

  param.push_back(camera_pam);
}

bool CameraManager::start_camera(int index)
{
  if (index < 0 || static_cast<size_t>(index) >= devices_.size()) {
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
  if (index < 0 || static_cast<size_t>(index) >= devices_.size()) {
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
  if (index < 0 || static_cast<size_t>(index) >= devices_.size()) {
    std::cout << "[WARNING] [" << logger_ << "]: please create the camera at first." << std::endl;
    return false;
  }

  if (devices_[index] == nullptr) {
    std::cout << "[WARNING] [" << logger_ << "]: the camera is not work now." << std::endl;
    return false;
  }

  auto type = devices_[index]->get_type();

  ICameraListener * listener = nullptr;
  if (type == CameraType::QCarCam) {
    std::cout << "[INFO] [" << logger_ << "]: create qcarcam listener!!" << std::endl;
    listener = new QCarCamListener(image_cb);
  }

  if (listener == nullptr) {
    std::cout << "[WARNING] [" << logger_ << "]: unsupported camera type for this build." << std::endl;
    return false;
  }

  listeners_[index].push_back(listener);

  devices_[index]->add_listener(listener);
  return true;
}

}  // namespace qrb_camera
