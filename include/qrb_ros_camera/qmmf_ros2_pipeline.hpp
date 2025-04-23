/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 *​​​​ ​Changes from Qualcomm Innovation Center, Inc. are provided under the following
 *license: Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef __QMMF_ROS2_PIPELINE_H__
#define __QMMF_ROS2_PIPELINE_H__
#include "camera_ros2_common.hpp"
#include "camera_ros2_config.hpp"
#include "qmmf_camera_common.hpp"
#include "qmmf_camera_parameters.hpp"
#include "rclcpp/rclcpp.hpp"
using namespace std;
typedef int32_t status_t;
// #define USE_APPSINK 1
#define GET_REFCNT(e) (((GObject *)e)->ref_count)
namespace qrb_ros::camera
{
enum OutputFormatType
{
  YuvFormat,
  RawFormat,
  JpegFormat
};
enum CamFunction
{
  CamFuncHires = 0,        // hi-res
  CamFuncLeftSensor = 3,   // left ov
  CamFuncTracking = 1,     // tracking ov
  CamFuncRightSensor = 4,  // right ov
  CamFuncStereo = 2,       // _todo, stereo
  CamFuncTof = 5,
  CamFuncMax,
};
enum AppLoglevel
{
  CamLogSilent = 0,
  CamLogError = 1,
  CamLogInfo = 2,
  CamLogDebug = 3,
  CamLogMax,
};
struct CameraCaps
{
  vector<ImageSize> p_sizes, v_sizes, pic_sizes, raw_pic_sizes;
  vector<string> focus_modes, wb_modes, iso_modes;
  vector<string> sharpness_edge_modes, tonemap_modes;
  Range brightness, sharpness, contrast;
  vector<Range> preview_fps_ranges;
  vector<string> preview_formats;
  string raw_size;
  Range gain_range;
};
struct TestConfig
{
  string output_format;
  qrb_ros::camera::ImageSize p_size;
  qrb_ros::camera::ImageSize v_size;
  qrb_ros::camera::ImageSize pic_size;
  int fps;
  AppLoglevel log_level;
};
class QmmfRos2Pipeline : public ICameraListener
{
public:
  QmmfRos2Pipeline();
  QmmfRos2Pipeline(CameraRos2Config * cfg);
  virtual ~QmmfRos2Pipeline();
  static QmmfRos2Pipeline * create_instance(CameraRos2Config * cfg);
  void destroy();
  void register_publish(CameraRos2MsgPublishFrameFunc publish);
  // bool update_params(CameraRos2Config *cfg);
  /* listener methods */
  virtual void on_error();
  virtual void on_preview_frame(ICameraFrame * frame);
  virtual void on_video_frame(ICameraFrame * frame);
  virtual void on_picture_frame(ICameraFrame * frame);

private:
  ICameraDevice * camera_;
  CameraParams params_;
  qrb_ros::camera::ImageSize p_size_, v_size_, pic_size_;
  CameraCaps caps_;
  TestConfig config_;
  // std::mutex m_mutexParamUpdate;
  bool is_pic_done_;
  int32_t init(CameraRos2Config * cfg);
  void deinit();
  int32_t start();
  void stop();
  int print_capabilities();
  int set_parameters();
  int set_fps_index(TestConfig & cfg, int & pFpsIdx, int & vFpsIdx);
  int set_default_config();
  int parse_format(string & previewFormat);
  CameraRos2Config * m_p_config_;
  int32_t m_cur_frame_id_;
  uint64_t m_base_time_;
  CameraRos2MsgPublishFrameFunc m_p_publish_;
};
}  // namespace qrb_ros::camera
#endif
