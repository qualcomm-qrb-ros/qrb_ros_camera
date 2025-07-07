/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef QRB_CAMERA__CAMERA_MANAGER_HPP_
#define QRB_CAMERA__CAMERA_MANAGER_HPP_

#include <memory>
#include <mutex>

#include "qrb_camera/camera_interface.hpp"
#include "qrb_camera/qmmf_camera.hpp"
#include "qrb_camera/qrb_camera_common.hpp"

namespace qrb_camera
{
using ImageCallback = std::function<void(std::unique_ptr<CameraFrame> frame)>;

class CameraManager
{
public:
  ~CameraManager();

  int create_camera(CameraType type, uint32_t camera_id);
  bool set_camera_parameter(int index, CameraConfigure & param);

  bool start_camera(int index);
  void stop_camera(int index);

  bool register_callback(int index, ImageCallback image_cb);

  bool destroy_camera(int index);

private:
  void convert_detail_to_param(int index, std::vector<CameraParam> & param);

  std::mutex mtx_;
  std::vector<ICameraDevice *> devices_;

  std::vector<std::shared_ptr<CameraConfigure>> params_;
  std::vector<std::vector<ICameraListener *>> listeners_;

  std::vector<CameraType> types_;

  std::string logger_ = "CameraManager";
};
}  // namespace qrb_camera
#endif