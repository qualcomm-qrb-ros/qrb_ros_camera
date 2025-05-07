/*
 * Copyright (c) 2015, 2017, The Linux Foundation. All rights reserved.
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
 *
 */
/*
 *​​​​ ​Changes from Qualcomm Innovation Center, Inc. are provided under the following
 *license: Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include "qrb_ros_camera/qmmf_camera.hpp"

#include "qrb_ros_camera/qmmf_camera_common.hpp"
namespace qrb_ros::camera
{
static pthread_mutex_t halMutex = PTHREAD_MUTEX_INITIALIZER;
static std::vector<int> g_openedCameras;
/**
 * check if given camera ID is already open
 *
 * @param camera_id
 *
 * @return bool
 */
static bool is_open(int camera_id)
{
  bool ret = false;
  pthread_mutex_lock(&halMutex);
  for (unsigned int i = 0; i < g_openedCameras.size(); i++) {
    if (g_openedCameras[i] == camera_id) {
      ret = true;
      break;
    }
  }
  pthread_mutex_unlock(&halMutex);
  return ret;
}

// map<fd, frame>
std::map<int, qmmfCameraFrame *> qmmfCameraFrame::refs_fd_frame_map_;
std::mutex qmmfCameraFrame::refs_map_mutex_;

qmmfCameraFrame::~qmmfCameraFrame()
{
  status_t ret = NO_ERROR;
  // Return buffer back to recorder service.
  if (type_ == CAMERA_QMMF_TRACK_PREVIEW || type_ == CAMERA_QMMF_TRACK_VIDEO) {
    ret = recorder_->ReturnTrackBuffer(this->track_id_, this->buffers_);
  } else if (type_ == CAMERA_QMMF_TRACK_PICTURE) {
    ret = recorder_->ReturnImageCaptureBuffer(this->camera_id_, this->buffer_);
  }
  if (ret != NO_ERROR) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "Return image buffer failed - %d", ret);
  }
}

uint32_t qmmfCameraFrame::acquire_ref()
{
  qmmfCameraFrame::refs_map_mutex_.lock();
  qmmfCameraFrame::refs_fd_frame_map_.emplace(this->fd, this);
  qmmfCameraFrame::refs_map_mutex_.unlock();
  refs_mutex_.lock();
  refs_++;
  refs_mutex_.unlock();
  CAM_DBG("TS = %llu ref = %u", this->timestamp, refs_);
  return refs_;
}

uint32_t qmmfCameraFrame::release_ref()
{
  refs_mutex_.lock();

  if (refs_ <= 0) {
    refs_ = 0;
    goto bail;
  }
  refs_--;
  if (0 == refs_) {
    RCLCPP_DEBUG(
        rclcpp::get_logger("qrb_ros_camera"), "Delete TS = %llu ref = %u ", this->timestamp, refs_);
    goto delete_frame;
  }
bail:
  RCLCPP_DEBUG(rclcpp::get_logger("qrb_ros_camera"), "TS = %llu ref = %u ", this->timestamp, refs_);
  refs_mutex_.unlock();
  return refs_;
delete_frame:
  qmmfCameraFrame::refs_map_mutex_.lock();
  std::map<int, qmmfCameraFrame *>::iterator it =
      qmmfCameraFrame::refs_fd_frame_map_.find(this->fd);
  if (it != qmmfCameraFrame::refs_fd_frame_map_.end()) {
    qmmfCameraFrame::refs_fd_frame_map_.erase(it);
  } else {
    RCLCPP_WARN(rclcpp::get_logger("qrb_ros_camera"), "Frame not found in ref_fd_frame_map");
    qmmfCameraFrame::refs_map_mutex_.unlock();
    refs_mutex_.unlock();
    return EXIT_SUCCESS;
  }
  qmmfCameraFrame::refs_map_mutex_.unlock();
  refs_mutex_.unlock();
  delete this;
  return EXIT_SUCCESS;
}

status_t release_all_frames()
{
  size_t refs_fdframe_map_size;
  int wait_count = 0;
  qmmfCameraFrame::refs_map_mutex_.lock();
  refs_fdframe_map_size = qmmfCameraFrame::refs_fd_frame_map_.size();
  qmmfCameraFrame::refs_map_mutex_.unlock();
  while (refs_fdframe_map_size > 0) {
    RCLCPP_WARN(rclcpp::get_logger("qrb_ros_camera"), "Waiting for buffers (%d) to be released \n",
        refs_fdframe_map_size);
    usleep(RELEASE_REF_BUF_WAIT_US);
    wait_count++;
    if (wait_count > RELEASE_REF_BUF_WAIT_MAX_ATTEMPT) {
      qmmfCameraFrame::refs_map_mutex_.lock();
      for (std::map<int, qmmfCameraFrame *>::iterator it =
               qmmfCameraFrame::refs_fd_frame_map_.begin();
           it != qmmfCameraFrame::refs_fd_frame_map_.end(); it++) {
        RCLCPP_INFO(rclcpp::get_logger("qrb_ros_camera"), "Force release pending buffers : FD = %d",
            it->first);
        delete it->second;
        qmmfCameraFrame::refs_fd_frame_map_.erase(it);
      }
      qmmfCameraFrame::refs_map_mutex_.unlock();
      break;
    }
    qmmfCameraFrame::refs_map_mutex_.lock();
    refs_fdframe_map_size = qmmfCameraFrame::refs_fd_frame_map_.size();
    qmmfCameraFrame::refs_map_mutex_.unlock();
  }
  return EXIT_SUCCESS;
}

qmmfCAMERA::qmmfCAMERA() : is_preview_running_(false) {}

int qmmfCAMERA::init(uint32_t idx)
{
  RCLCPP_INFO(rclcpp::get_logger("qrb_ros_camera"), "init camera");
  status_t ret = EXIT_SUCCESS;
  camera_id_ = idx;
  ret = qmmf_recorder_connect();
  if (ret != NO_ERROR) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "qmmf_recorder_connect failed - %d ", ret);
    goto bail;
  }
  ret = qmmf_start_camera();
  if (ret != 0) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "qmmf_start_camera failed - %d ", ret);
    goto bail;
  }
  for (uint32_t i = CAMERA_QMMF_TRACK_PREVIEW; i < CAMERA_QMMF_TRACK_MAX; i++) {
    qmmfTrackArray_[i].init(
        this, &recorder_, camera_id_, i, &(qmmf_camera_params_.track_params_ptr[i]));
  }
bail:
  return ret;
}

qmmfCAMERA::~qmmfCAMERA() {}

int qmmfCAMERA::set_parameters(const CameraParams & params)
{
  status_t ret = EXIT_SUCCESS;
  int32_t status;
  qmmf_camera_params_.mutex_lock();
  if (qmmf_camera_params_.static_session_meta_info_updated_flag == false) {
    goto bail;
  } else {
    qmmf_camera_params_.static_session_meta_info_updated_flag = false;
  }
  status = recorder_.SetCameraParam(camera_id_, qmmf_camera_params_.static_session_meta_info);
  if (0 != status) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "SetCameraParam Failed \n");
    ret = EXIT_FAILURE;
  } else {
    RCLCPP_DEBUG(rclcpp::get_logger("qrb_ros_camera"), "SetCameraParam Success \n");
  }
bail:
  qmmf_camera_params_.mutex_unlock();
  return ret;
}

int qmmfCAMERA::get_parameters(uint8_t * buf, uint32_t buf_size, int * bufSizeRequired)
{
  return EXIT_SUCCESS;
}

void * qmmfCAMERA::get_parameters()
{
  return &qmmf_camera_params_;
}

void qmmfCAMERA::add_listener(ICameraListener * listener)
{
  /* check if this listener is already added, to avoid adding duplicates */
  for (unsigned int i = 0; i < listeners_.size(); i++) {
    if (listener == listeners_[i]) {
      RCLCPP_WARN(rclcpp::get_logger("qrb_ros_camera"), "this listener is already added");
      return;
    }
  }
  listeners_.push_back(listener);
}

void qmmfCAMERA::remove_listener(ICameraListener * listener)
{
  /* erase if this listener is added */
  for (unsigned int i = 0; i < listeners_.size(); i++) {
    if (listener == listeners_[i]) {
      listeners_.erase(listeners_.begin() + i);
      RCLCPP_INFO(rclcpp::get_logger("qrb_ros_camera"), "remove listener successful.");
      return;
    }
  }
  RCLCPP_WARN(rclcpp::get_logger("qrb_ros_camera"), "listener can't find");
  return;
}

int qmmfCAMERA::start_preview()
{
  status_t ret = EXIT_SUCCESS;
  RCLCPP_INFO(rclcpp::get_logger("qrb_ros_camera"), "Enter, is preview already on: %d",
      is_preview_running_);
  if (is_preview_running_ == true) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "Preview already running P_on = %d",
        is_preview_running_);
    return EXIT_FAILURE;
  }
  ret = qmmf_start_track(CAMERA_QMMF_TRACK_PREVIEW);
  is_preview_running_ = true;
  RCLCPP_INFO(rclcpp::get_logger("qrb_ros_camera"), "start preview done, ret: %d", ret);
  return ret;
}

void qmmfCAMERA::stop_preview()
{
  status_t ret;
  RCLCPP_DEBUG(rclcpp::get_logger("qrb_ros_camera"), "Enter, is preview already on: %d",
      is_preview_running_);
  if (is_preview_running_ != true) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "Preview not started");
    return;
  }
  ret = qmmfTrackArray_[CAMERA_QMMF_TRACK_PREVIEW].delete_qmmf_track();
  if (ret != NO_ERROR) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "Delete qmmf track failed: %d ", ret);
  }
  is_preview_running_ = false;
  qmmf_stop_camera();
  qmmf_recorder_disconnect();
  RCLCPP_DEBUG(rclcpp::get_logger("qrb_ros_camera"), "stop preview done");
  return;
}

int ICameraDevice::create_instance(int index, ICameraDevice ** device)
{
  RCLCPP_DEBUG(rclcpp::get_logger("qrb_ros_camera"), "create camera device instance");
  int rc = 0;
  qmmfCAMERA * me = nullptr;
  if (is_open(index) == true) {
    rc = EBUSY;
    goto bail;
  }
  me = new qmmfCAMERA;
  if (me == nullptr) {
    rc = ENOMEM;
    goto bail;
  }
  rc = me->init(index);
  if (rc != 0) {
    rc = ENODEV;
    goto bail;
  }
  /* add entry to openCameras vector */
  pthread_mutex_lock(&halMutex);
  g_openedCameras.push_back(index);
  pthread_mutex_unlock(&halMutex);
  *device = static_cast<ICameraDevice *>(me);
  RCLCPP_DEBUG(rclcpp::get_logger("qrb_ros_camera"), "Camera device create success");
  return 0;
bail:
  delete me;
  RCLCPP_DEBUG(rclcpp::get_logger("qrb_ros_camera"), "Camera device create failed");
  return rc;
}

void ICameraDevice::delete_instance(ICameraDevice ** device)
{
  RCLCPP_DEBUG(rclcpp::get_logger("qrb_ros_camera"), "delete camera device instance");
  qmmfCAMERA * me = static_cast<qmmfCAMERA *>(*device);
  if (me == nullptr) {
    RCLCPP_INFO(rclcpp::get_logger("qrb_ros_camera"), "Already deleted");
    return;
  }
  /* erase entry from openCamera vector */
  pthread_mutex_lock(&halMutex);
  for (size_t i = 0; i < g_openedCameras.size(); i++) {
    if (g_openedCameras[i] == me->get_id()) {
      g_openedCameras.erase(g_openedCameras.begin() + i);
      break;
    }
  }
  pthread_mutex_unlock(&halMutex);
  if (me != nullptr) {
    delete me;
    *device = nullptr;
  }
  RCLCPP_DEBUG(rclcpp::get_logger("qrb_ros_camera"), "delete camera device instance done");
  return;
}

status_t qmmfCAMERA::qmmf_recorder_connect()
{
  RCLCPP_DEBUG(rclcpp::get_logger("qrb_ros_camera"), "connect camera server");
  ::qmmf::recorder::RecorderCb recorder_status_cb;
  recorder_status_cb.event_cb = [&](qmmf::recorder::EventType event_type, void * event_data,
                                    size_t event_data_size) {
    qmmfCAMERA::connect_event_cb(event_type, event_data, event_data_size);
  };
  auto ret = recorder_.Connect(recorder_status_cb);
  return ret;
}

status_t qmmfCAMERA::qmmf_recorder_disconnect()
{
  auto ret = recorder_.Disconnect();
  return ret;
}

status_t qmmfCAMERA::qmmf_start_camera()
{
  status_t ret;
  RCLCPP_DEBUG(rclcpp::get_logger("qrb_ros_camera"), "start camera");
  CameraResultCb result_cb;
  result_cb = [&](uint32_t camera_id, const CameraMetadata & result) {
    camera_metadata_cb(camera_id, result);
  };
  ret = recorder_.StartCamera(
      camera_id_, DEFAULT_SENSOR_FRAME_RATE, qmmf_camera_params_.start_params, result_cb);
  if (NO_ERROR != ret) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "StartCamera Failed!!");
    goto bail;
  }
  ret = recorder_.GetDefaultCaptureParam(camera_id_, qmmf_camera_params_.static_default_meta_info);
  if (NO_ERROR != ret) {
    RCLCPP_ERROR(
        rclcpp::get_logger("qrb_ros_camera"), "Unable to query default capture parameters!\n");
    goto bail;
  }
bail:
  RCLCPP_DEBUG(rclcpp::get_logger("qrb_ros_camera"), "finish start camera");
  return ret;
}

status_t qmmfCAMERA::qmmf_stop_camera()
{
  auto ret = recorder_.StopCamera(camera_id_);
  if (ret != 0) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "StopCamera Failed!!");
    return ret;
  }
  qmmf_camera_params_.mutex_lock();
  qmmf_camera_params_.static_default_meta_info.clear();
  qmmf_camera_params_.static_session_meta_info.clear();
  qmmf_camera_params_.mutex_unlock();
  return ret;
}

status_t qmmfCAMERA::qmmf_start_track(FrameType trackType)
{
  status_t ret = EXIT_SUCCESS;
  ret = qmmfTrackArray_[trackType].create_qmmf_track();
  if (NO_ERROR != ret) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "create_qmmf_track failed\n");
    goto bail;
  };
bail:
  return ret;
}

status_t QMMFTrack::create_qmmf_track()
{
  status_t ret = EXIT_SUCCESS;
  track_ids.emplace(track_id_);
  ::qmmf::recorder::TrackCb video_track_cb;
  ::qmmf::recorder::VideoExtraParam extraparam;
  video_track_cb.data_cb = [&](uint32_t track_id, std::vector<BufferDescriptor> buffers,
                               std::vector<::qmmf::BufferMeta> meta_buffers) {
    qmmf_camera_ptr_->qmmf_track_data_cb(track_id, buffers, meta_buffers);
  };
  video_track_cb.event_cb = [&](uint32_t track_id, qmmf::recorder::EventType event_type,
                                void * event_data, size_t data_size) {
    qmmf_camera_ptr_->qmmf_track_event_cb(track_id, event_type, event_data, data_size);
  };
  ret = recorder_ptr_->CreateVideoTrack(track_id_, track_params_, extraparam, video_track_cb);
  if (NO_ERROR != ret) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "CreateVideoTrack failed\n");
    goto bail;
  }
  ret = recorder_ptr_->StartVideoTracks(track_ids);
bail:
  return ret;
}

status_t QMMFTrack::delete_qmmf_track()
{
  status_t ret = EXIT_SUCCESS;
  ret = recorder_ptr_->StopVideoTracks(track_ids);
  if (ret != NO_ERROR) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "Stop video Track fail \n");
    return ret;
  }
  ret = recorder_ptr_->DeleteVideoTrack(track_id_);
  if (ret != NO_ERROR) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"),
        "Delete video track failed: %d, track id: %u \n", ret, track_id_);
    return ret;
  } else {
    RCLCPP_INFO(rclcpp::get_logger("qrb_ros_camera"),
        "Delete video track successful: %d, track id: %u \n", ret, track_id_);
  }
  return ret;
}

// metadata callback
void qmmfCAMERA::camera_metadata_cb(uint32_t camera_id, const CameraMetadata & result)
{
  uint32_t meta_frame_index = result.find(ANDROID_REQUEST_FRAME_COUNT).data.i32[0];
  result_metadata_cache_mutex_.lock();
  if (result_metadata_cache_.size() >= 120) {
    result_metadata_cache_.erase(result_metadata_cache_.begin());
  }
  auto it = result_metadata_cache_.find(meta_frame_index);
  if (it != result_metadata_cache_.end()) {
    it->second = result;
  } else {
    result_metadata_cache_.emplace(std::make_pair(meta_frame_index, result));
  }
  result_metadata_cache_mutex_.unlock();
}

void qmmfCAMERA::connect_event_cb(qmmf::recorder::EventType event_type,
    void * event_data,
    size_t event_data_size)
{
  if (event_type == EventType::kCameraError) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "Found CameraError");
  }
}

void qmmfCAMERA::session_callback_handler(qmmf::recorder::EventType event_type,
    void * event_data,
    size_t event_data_size)
{
  RCLCPP_DEBUG(rclcpp::get_logger("qrb_ros_camera"), "Enter session_callback_handler");
}

void qmmfCAMERA::qmmf_track_event_cb(uint32_t track_id,
    qmmf::recorder::EventType event_type,
    void * event_data,
    size_t event_data_size)
{
  RCLCPP_DEBUG(rclcpp::get_logger("qrb_ros_camera"), "Enter qmmf_track_event_cb");
}

void qmmfCAMERA::qmmf_track_data_cb(uint32_t track_id,
    std::vector<::qmmf::BufferDescriptor> buffers,
    std::vector<::qmmf::BufferMeta> meta_buffers)
{
  // TODO: CameraMetada should associate with result_metadata_cache_.
  CameraMetadata result;
  /* Here the assumption is that each callback returns only buffer in the
   * BufferDescriptor*/
  qmmfCameraFrame::dispatch_frame(
      this->listeners_, &recorder_, buffers, meta_buffers, result, camera_id_, track_id);
}

void qmmfCameraFrame::dispatch_frame(std::vector<ICameraListener *> listeners,
    qmmf::recorder::Recorder * recorder,
    std::vector<BufferDescriptor> & buffers,
    std::vector<::qmmf::BufferMeta> & meta_buffers,
    CameraMetadata & frameMetadata,
    uint32_t camera_id,
    uint32_t track_id)
{
  RCLCPP_DEBUG(rclcpp::get_logger("qrb_ros_camera"), "Enter");
  if (meta_buffers.size() == 0 || buffers.size() == 0) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "Error : buffer is empty ");
    return;
  }
  qmmfCameraFrame * frame = new qmmfCameraFrame(buffers.begin()->fd, buffers.begin()->size);
  if (frame == nullptr) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "new frame memory allocation failed");
    return;
  }
  frame->recorder_ = recorder;
  frame->buffers_ = buffers;
  frame->meta_buffers_ = meta_buffers;
  // do not used
  // frame->frame_info_metadata_ = frameMetadata;
  frame->stride = meta_buffers.begin()->planes[0].stride;
  frame->slice = meta_buffers.begin()->planes[0].scanline;
  frame->camera_id_ = camera_id;
  frame->track_id_ = track_id;
  frame->buf_id_ = buffers.begin()->buf_id;
  frame->timestamp = buffers.begin()->timestamp;
  frame->data = (uint8_t *)buffers.begin()->data;
  frame->size = buffers.begin()->size;
  frame->fd = buffers.begin()->fd;
  frame->acquire_ref();
  for (unsigned int i = 0; i < listeners.size(); i++) {
    switch (frame->track_id_) {
      case CAMERA_QMMF_TRACK_PREVIEW:
        frame->type_ = CAMERA_QMMF_TRACK_PREVIEW;
        listeners[i]->on_preview_frame(static_cast<ICameraFrame *>(frame));
        break;
      case CAMERA_QMMF_TRACK_VIDEO:
        frame->type_ = CAMERA_QMMF_TRACK_VIDEO;
        listeners[i]->on_video_frame(static_cast<ICameraFrame *>(frame));
        break;
      case CAMERA_QMMF_TRACK_PICTURE:
        frame->type_ = CAMERA_QMMF_TRACK_PICTURE;
        listeners[i]->on_picture_frame(static_cast<ICameraFrame *>(frame));
        break;
      default:
        RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "unsupported track_id %d", track_id);
    }
  }
  frame->release_ref();
}

qmmfCameraFrame::qmmfCameraFrame(int fd, uint32_t size) : recorder_(nullptr)
{
  RCLCPP_DEBUG(rclcpp::get_logger("qrb_ros_camera"), "Enter qmmfCameraFrame");
}

QMMFTrack::QMMFTrack()
{
  track_id_ = CAMERA_QMMF_TRACK_PREVIEW;
  memset(&track_params_, 0x0, sizeof track_params_);
  track_params_.camera_id = 0;
  track_params_.width = DEFAULT_SENSOR_WIDTH;
  track_params_.height = DEFAULT_SENSOR_HEIGHT;
  track_params_.framerate = DEFAULT_SENSOR_FRAME_RATE;
  track_params_.format = VideoFormat::kNV12;
  track_params_.xtrabufs = 0;
}

int QMMFTrack::init(qmmfCAMERA * qmmf_camera_ptr,
    Recorder * recorder_ptr,
    uint32_t camera_id,
    uint32_t track_id,
    VideoTrackParam ** track_params_ptr)
{
  int ret = EXIT_SUCCESS;
  qmmf_camera_ptr_ = qmmf_camera_ptr;
  if (qmmf_camera_ptr == nullptr || recorder_ptr == nullptr) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), ": Error ");
    ret = EXIT_FAILURE;
    return ret;
  }
  recorder_ptr_ = recorder_ptr;
  track_params_.camera_id = camera_id;
  track_id_ = track_id;
  *(track_params_ptr) = &track_params_;
  if (track_params_ptr == nullptr) {
    RCLCPP_ERROR(rclcpp::get_logger("qrb_ros_camera"), "Error");
    ret = EXIT_FAILURE;
    return ret;
  }
  return ret;
}
}  // namespace qrb_ros::camera