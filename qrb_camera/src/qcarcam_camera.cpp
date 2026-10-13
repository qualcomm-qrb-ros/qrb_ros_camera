/*
 *​​​​ Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

// =============================================================================
// QCarCamCamera – QCarCam submit-request mode, multi-stream implementation.
//
// Key differences from the previous single-stream version:
//   - set_param() creates one QCarCamStream per track_param entry.
//   - start_camera() calls QCarCamSetBuffers once per stream (multiple
//     buffer lists per handle), then one QCarCamReserve + QCarCamStart.
//   - on_frame_ready() routes by payload->frameInfo.id to the matching stream.
//   - resubmit_buffer() is called by streams via ResubmitFn lambda; it is
//     serialised by request_mtx_ and ALWAYS increments requestId regardless
//     of success (matches qcarcam_test.cpp, fixes the original qcc.cpp bug).
//   - Buffers are allocated via Linux DMA heap (/dev/dma_heap/system).
//     No GBM or DRM dependency required.
// =============================================================================

#include "qrb_camera/qcarcam_camera.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <thread>

namespace qrb_camera
{

// ---------------------------------------------------------------------------
// Static event callback (registered with QCarCamRegisterEventCallback)
// ---------------------------------------------------------------------------
static QCarCamRet_e qcarcam_event_cb(
    const QCarCamHndl_t           hndl,
    const uint32_t                event_id,
    const QCarCamEventPayload_t * payload,
    void *                        private_data)
{
  (void)hndl;
  auto * self = static_cast<QCarCamCamera *>(private_data);

  switch (event_id) {
    case QCARCAM_EVENT_FRAME_READY:
      self->on_frame_ready(payload);
      break;
    case QCARCAM_EVENT_ERROR:
      self->on_error(payload);
      break;
    case QCARCAM_EVENT_INPUT_SIGNAL:
      std::cout << "[INFO] [QCarCamCamera]: INPUT_SIGNAL event" << std::endl;
      break;
    default:
      std::cout << "[WARN] [QCarCamCamera]: unhandled event " << event_id << std::endl;
      break;
  }
  return QCARCAM_RET_OK;
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
QCarCamCamera::QCarCamCamera(uint32_t camera_id)
  : ICameraDevice(camera_id)
{
  type_       = CameraType::QCarCam;
  is_working_ = false;
}

QCarCamCamera::~QCarCamCamera()
{
  if (is_working_) {
    stop_camera();
  }
}

// ---------------------------------------------------------------------------
// set_param – store track stream parameters; called by CameraManager before
// start_camera().  Each entry in track_params becomes one QCarCamStream.
// Snapshot-type params are ignored (requirement: no snapshot for QCarCam).
// ---------------------------------------------------------------------------
void QCarCamCamera::set_param(std::vector<CameraParam> & param)
{
  if (param.empty() || param[0].track_params.empty()) {
    std::cout << "[WARN] [" << logger_ << "]: set_param: no track streams provided" << std::endl;
    return;
  }

  stream_params_        = param[0].track_params;
  isp_config_.id        = 0;
  isp_config_.cameraId  = 0;
  isp_config_.usecaseId = static_cast<QCarCamIspUsecase_e>(param[0].isp_usecase);

  std::cout << "[INFO] [" << logger_ << "]: set_param: " << stream_params_.size()
            << " stream(s)" << std::endl;
  for (size_t i = 0; i < stream_params_.size(); ++i) {
    std::cout << "[INFO] [" << logger_ << "]:   stream[" << i << "] '"
              << stream_params_[i].name << "' "
              << stream_params_[i].width << "x" << stream_params_[i].height
              << " @" << stream_params_[i].frame_rate << "fps"
              << " fmt=" << stream_params_[i].format << std::endl;
  }
}

// ---------------------------------------------------------------------------
// init – QCarCamInitialize (called once before start_camera)
// ---------------------------------------------------------------------------
bool QCarCamCamera::init()
{
  if (!wait_for_qcx()) {
    std::cout << "[ERROR] [" << logger_ << "]: QCX server not ready" << std::endl;
    return false;
  }

  QCarCamInit_t init_params = {};
  init_params.apiVersion = QCARCAM_VERSION;

  QCarCamRet_e ret = QCarCamInitialize(&init_params);
  if (ret != QCARCAM_RET_OK) {
    std::cout << "[ERROR] [" << logger_ << "]: QCarCamInitialize failed: " << ret << std::endl;
    return false;
  }

  std::cout << "[INFO] [" << logger_ << "]: QCarCamInitialize OK" << std::endl;
  is_initialized_ = true;
  return true;
}

// ---------------------------------------------------------------------------
// start_camera – full 9-step setup sequence (per qcarcam_test.cpp order)
// ---------------------------------------------------------------------------
int QCarCamCamera::start_camera()
{
  if (stream_params_.empty()) {
    std::cout << "[ERROR] [" << logger_ << "]: no stream params – call set_param() first"
              << std::endl;
    return -1;
  }

  QCarCamRet_e ret;

  // Shared cleanup lambda – called on any failure path
  auto cleanup = [this]() {
    for (auto & s : streams_) {
      if (s) { s->free_buffers(); }
    }
    streams_.clear();
    if (handle_ != QCARCAM_HNDL_INVALID) {
      QCarCamStop(handle_);     // may fail if Start not reached yet — ignore
      QCarCamRelease(handle_);  // may fail if Reserve not reached yet — ignore
      QCarCamClose(handle_);
      handle_ = QCARCAM_HNDL_INVALID;
    }
  };

  // ------------------------------------------------------------------
  // Step 1: QueryInputs – verify camera is present (BEFORE Open).
  //   Two-step: first call with nullptr to get count, then fill.
  //   Matches qcarcam_test.cpp order.
  // ------------------------------------------------------------------
  if (!query_and_verify_input()) {
    std::cout << "[ERROR] [" << logger_ << "]: query_and_verify_input failed" << std::endl;
    return -1;
  }

  // ------------------------------------------------------------------
  // Step 2: Open QCarCam handle (with REQUEST_MODE flag)
  // ------------------------------------------------------------------
  QCarCamOpen_t open_params = {};
  open_params.opMode              = QCARCAM_OPMODE_ISP;
  open_params.flags              |= QCARCAM_OPEN_FLAGS_REQUEST_MODE;

  // config for each stream
  const uint32_t num_contexts = static_cast<uint32_t>(stream_params_.size());
  open_params.numInputs = num_contexts;
  for (uint32_t i = 0; i < num_contexts && i < QCARCAM_MAX_INPUT_STREAMS; ++i) {
    open_params.inputs[i].inputId   = camera_id_;
    open_params.inputs[i].srcId     = 0;
    open_params.inputs[i].inputMode = i;
  }

  for (uint32_t i = 0; i < QCARCAM_MAX_OPEN_RETRY; ++i) {
    ret = QCarCamOpen(&open_params, &handle_);
    if (handle_ != QCARCAM_HNDL_INVALID) {
      std::cout << "[INFO] [" << logger_ << "]: QCarCamOpen OK handle=0x"
                << std::hex << handle_ << std::dec << std::endl;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  if (handle_ == QCARCAM_HNDL_INVALID) {
    std::cout << "[ERROR] [" << logger_ << "]: QCarCamOpen failed after retries" << std::endl;
    cleanup();
    return -1;
  }

  // ------------------------------------------------------------------
  // Step 3: SetParam – ISP usecase (configured in set_param)
  // ------------------------------------------------------------------
  ret = QCarCamSetParam(handle_,
                        QCARCAM_STREAM_CONFIG_PARAM_ISP_USECASE,
                        &isp_config_, sizeof(isp_config_));
  if (ret != QCARCAM_RET_OK) {
    std::cout << "[ERROR] [" << logger_ << "]: SetParam ISP_USECASE failed: " << ret << std::endl;
    cleanup();
    return -1;
  }
  std::cout << "[INFO] [" << logger_ << "]: SetParam ISP_USECASE="
            << isp_config_.usecaseId << " OK" << std::endl;

  // ------------------------------------------------------------------
  // Step 5: RegisterEventCallback
  // ------------------------------------------------------------------
  ret = QCarCamRegisterEventCallback(handle_, &qcarcam_event_cb, this);
  if (ret != QCARCAM_RET_OK) {
    std::cout << "[ERROR] [" << logger_ << "]: RegisterEventCallback failed: " << ret << std::endl;
    cleanup();
    return -1;
  }

  // ------------------------------------------------------------------
  // Step 4: SetParam – EVENT_MASK
  // ------------------------------------------------------------------
  {
    uint32_t mask = QCARCAM_EVENT_FRAME_READY |
                    QCARCAM_EVENT_INPUT_SIGNAL |
                    QCARCAM_EVENT_ERROR;
    ret = QCarCamSetParam(handle_,
                          QCARCAM_STREAM_CONFIG_PARAM_EVENT_MASK,
                          &mask, sizeof(mask));
    if (ret != QCARCAM_RET_OK) {
      std::cout << "[ERROR] [" << logger_ << "]: SetParam EVENT_MASK failed: " << ret << std::endl;
      cleanup();
      return -1;
    }
  }

  // ------------------------------------------------------------------
  // Step 5: Allocate GBM buffers and call QCarCamSetBuffers per stream.
  //
  // streams_ is resized BEFORE allocate_buffers() to prevent any
  // vector reallocation that would invalidate buf_list_.pBuffers
  // (which points into each stream's internal buffers_[] array).
  //
  // buf_list_.id is set to (stream_index + 1) before QCarCamSetBuffers so
  // each stream has a unique bufferlist id and FRAME_READY routing works.
  // ------------------------------------------------------------------
  {
    auto resubmit_fn = [this](uint32_t blid, uint32_t idx) {
      resubmit_buffer(blid, idx);
    };

    streams_.clear();

    for (size_t s = 0; s < stream_params_.size(); ++s) {
      streams_.push_back(std::make_unique<QCarCamStream>());
      auto & st = *streams_.back();

      if (!st.init(stream_params_[s], camera_id_, resubmit_fn)) {
        std::cout << "[ERROR] [" << logger_ << "]: stream[" << s << "].init failed" << std::endl;
        cleanup();
        return -1;
      }

      if (!st.allocate_buffers()) {
        std::cout << "[ERROR] [" << logger_ << "]: stream[" << s << "].allocate_buffers failed"
                  << std::endl;
        cleanup();
        return -1;
      }

      if (stream_params_[s].bufferlist_id == 0) {
        std::cout << "[ERROR] [" << logger_ << "]: stream[" << s << "] '"
                  << stream_params_[s].name << "' has zero bufferlist_id"
                  << std::endl;
        cleanup();
        return -1;
      }
      st.buf_list().id = stream_params_[s].bufferlist_id;

      ret = QCarCamSetBuffers(handle_, &st.buf_list());
      if (ret != QCARCAM_RET_OK) {
        std::cout << "[ERROR] [" << logger_ << "]: QCarCamSetBuffers stream[" << s
                  << "] failed: " << ret << std::endl;
        cleanup();
        return -1;
      }

      std::cout << "[INFO] [" << logger_ << "]: QCarCamSetBuffers stream[" << s
                << "] name='" << st.stream_name()
                << "' bufferlist_id=" << st.buf_list_id() << std::endl;
    }
  }

  // ------------------------------------------------------------------
  // Step 6: Reserve – AFTER all SetBuffers, BEFORE Start.
  //   Matches qcarcam_test.cpp.
  // ------------------------------------------------------------------
  ret = QCarCamReserve(handle_);
  if (ret != QCARCAM_RET_OK) {
    std::cout << "[ERROR] [" << logger_ << "]: QCarCamReserve failed: " << ret << std::endl;
    cleanup();
    return -1;
  }
  std::cout << "[INFO] [" << logger_ << "]: QCarCamReserve OK" << std::endl;

  // ------------------------------------------------------------------
  // Step 7: Start
  // ------------------------------------------------------------------
  ret = QCarCamStart(handle_);
  if (ret != QCARCAM_RET_OK) {
    std::cout << "[ERROR] [" << logger_ << "]: QCarCamStart failed: " << ret << std::endl;
    QCarCamRelease(handle_);
    cleanup();
    return -1;
  }
  std::cout << "[INFO] [" << logger_ << "]: QCarCamStart OK" << std::endl;

  // ------------------------------------------------------------------
  // Step 8: Pre-queue all buffer slots for every stream.
  //   requestId starts at 0 and is ALWAYS incremented regardless of
  //   success (matches qcarcam_test.cpp, avoids duplicate IDs).
  // ------------------------------------------------------------------
  {
    std::lock_guard<std::mutex> lk(request_mtx_);
    request_.requestId = 0;
  }

  for (auto & sp : streams_) {
    if (!sp->submit_initial()) {
      std::cout << "[ERROR] [" << logger_ << "]: submit_initial failed for stream '"
                << sp->stream_name() << "'" << std::endl;
      QCarCamStop(handle_);
      QCarCamRelease(handle_);
      cleanup();
      return -1;
    }
  }

  running_.store(true);
  is_working_ = true;

  std::cout << "[INFO] [" << logger_ << "]: camera " << camera_id_
            << " started with " << streams_.size() << " stream(s)" << std::endl;
  return 0;
}

// ---------------------------------------------------------------------------
// stop_camera
// ---------------------------------------------------------------------------
int QCarCamCamera::stop_camera()
{
  if (!is_working_) {
    return 0;
  }

  running_.store(false);

  QCarCamRet_e ret;

  ret = QCarCamStop(handle_);
  if (ret != QCARCAM_RET_OK) {
    std::cout << "[WARN] [" << logger_ << "]: QCarCamStop failed: " << ret << std::endl;
  }

  ret = QCarCamRelease(handle_);
  if (ret != QCARCAM_RET_OK) {
    std::cout << "[WARN] [" << logger_ << "]: QCarCamRelease failed: " << ret << std::endl;
  }

  ret = QCarCamClose(handle_);
  if (ret != QCARCAM_RET_OK) {
    std::cout << "[WARN] [" << logger_ << "]: QCarCamClose failed: " << ret << std::endl;
  }
  handle_ = QCARCAM_HNDL_INVALID;

  if (is_initialized_) {
    QCarCamUninitialize();
    is_initialized_ = false;
  }

  for (auto & s : streams_) {
    if (s) { s->free_buffers(); }
  }
  streams_.clear();

  is_working_ = false;
  std::cout << "[INFO] [" << logger_ << "]: camera " << camera_id_ << " stopped" << std::endl;
  return 0;
}

// ---------------------------------------------------------------------------
// add_listener / remove_listener
// ---------------------------------------------------------------------------
void QCarCamCamera::add_listener(ICameraListener * listener)
{
  std::lock_guard<std::mutex> lk(listener_mtx_);
  if (std::find(listeners_.begin(), listeners_.end(), listener) == listeners_.end()) {
    listeners_.push_back(listener);
  }
}

void QCarCamCamera::remove_listener(ICameraListener * listener)
{
  std::lock_guard<std::mutex> lk(listener_mtx_);
  auto it = std::find(listeners_.begin(), listeners_.end(), listener);
  if (it != listeners_.end()) {
    listeners_.erase(it);
  }
}

// ---------------------------------------------------------------------------
// on_frame_ready – event callback (runs on QCarCam's internal callback thread)
//
// Routes the FRAME_READY event to the correct QCarCamStream based on the
// bufferlist id carried in payload->frameInfo.id.
// ---------------------------------------------------------------------------
void QCarCamCamera::on_frame_ready(const QCarCamEventPayload_t * payload)
{
  if (!running_.load() || !payload) {
    return;
  }

  uint32_t blid    = payload->frameInfo.id;
  uint32_t buf_idx = payload->frameInfo.bufferIndex;
  uint64_t hw_ts   = payload->frameInfo.sofTimestamp.timestamp;

  // Snapshot the listener list (short lock, no callback under lock)
  std::vector<ICameraListener *> snapshot;
  {
    std::lock_guard<std::mutex> lk(listener_mtx_);
    snapshot = listeners_;
  }

  // Route to the stream whose bufferlist id matches
  for (auto & sp : streams_) {
    if (sp->buf_list_id() == blid) {
      sp->on_frame_ready(buf_idx, hw_ts, snapshot);
      return;
    }
  }

  std::cout << "[WARN] [" << logger_ << "]: FRAME_READY for unknown bufferlist_id="
            << blid << std::endl;
}

// ---------------------------------------------------------------------------
// on_error
// ---------------------------------------------------------------------------
void QCarCamCamera::on_error(const QCarCamEventPayload_t * payload)
{
  if (payload && payload->errInfo.errorId == QCARCAM_ERROR_FATAL) {
    std::cout << "[ERROR] [" << logger_ << "]: FATAL error, stopping" << std::endl;
    running_.store(false);
  } else {
    uint32_t err_id   = payload ? payload->errInfo.errorId   : 0;
    uint32_t err_code = payload ? payload->errInfo.errorCode : 0;
    std::cout << "[ERROR] [" << logger_ << "]: error id=" << err_id
              << " code=" << err_code << std::endl;
  }
}

// ---------------------------------------------------------------------------
// resubmit_buffer – submit one buffer slot back to the driver.
//   Called by QCarCamStream objects via the ResubmitFn lambda.
//   request_mtx_ serialises concurrent callers.
//   requestId is ALWAYS incremented regardless of success.
// ---------------------------------------------------------------------------
void QCarCamCamera::resubmit_buffer(uint32_t bufferlist_id, uint32_t buf_idx)
{
  std::lock_guard<std::mutex> lk(request_mtx_);
  request_.numStreamRequests               = 1;
  request_.streamRequests[0].bufferlistId  = bufferlist_id;
  request_.streamRequests[0].bufferIdx     = buf_idx;

  QCarCamRet_e ret = QCarCamSubmitRequest(handle_, &request_);
  ++request_.requestId;  // always increment (matches qcarcam_test.cpp)

  if (ret != QCARCAM_RET_OK) {
    std::cout << "[ERROR] [" << logger_ << "]: QCarCamSubmitRequest bl=" << bufferlist_id
              << " buf=" << buf_idx << " failed: " << ret << std::endl;
  }
}

// ===========================================================================
// Private helpers
// ===========================================================================

// ---------------------------------------------------------------------------
// wait_for_qcx – poll for QCX server mqueue (mirrors qcc.cpp WaitForQCX)
// ---------------------------------------------------------------------------
bool QCarCamCamera::wait_for_qcx()
{
  const int poll_ms  = 10;
  const int limit_ms = 4000;
  int elapsed = 0;

  while (elapsed < limit_ms) {
    if (access(QCARCAM_QCX_MQUEUE_PATH, F_OK) == 0) {
      std::cout << "[INFO] [" << logger_ << "]: QCX mqueue found" << std::endl;
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
    elapsed += poll_ms;
  }

  std::cout << "[ERROR] [" << logger_ << "]: QCX mqueue not found after "
            << limit_ms << " ms" << std::endl;
  return false;
}

// ---------------------------------------------------------------------------
// query_and_verify_input – two-step QueryInputs (matches qcarcam_test.cpp)
//   Step A: call with nullptr to get count.
//   Step B: allocate array and fill; retry until camera_id_ is found.
// ---------------------------------------------------------------------------
bool QCarCamCamera::query_and_verify_input()
{
  QCarCamRet_e ret;
  uint32_t num_inputs = 0;

  // Step A: get count
  ret = QCarCamQueryInputs(nullptr, 0, &num_inputs);
  if (ret != QCARCAM_RET_OK || num_inputs == 0) {
    std::cout << "[ERROR] [" << logger_ << "]: QCarCamQueryInputs (count) failed: ret="
              << ret << " count=" << num_inputs << std::endl;
    return false;
  }

  // Step B: fill with retry
  auto inputs = std::make_unique<QCarCamInput_t[]>(num_inputs);
  uint32_t filled = 0;

  for (uint32_t retry = 0; retry < QCARCAM_MAX_QUERY_RETRY; ++retry) {
    ret = QCarCamQueryInputs(inputs.get(), num_inputs, &filled);
    for (uint32_t i = 0; i < filled; ++i) {
      if (inputs[i].inputId == camera_id_) {
        std::cout << "[INFO] [" << logger_ << "]: found camera inputId="
                  << camera_id_ << std::endl;
        return true;
      }
    }
    std::this_thread::sleep_for(std::chrono::microseconds(QCARCAM_SENSOR_WAIT_US));
  }

  std::cout << "[ERROR] [" << logger_ << "]: camera inputId=" << camera_id_
            << " not found after retries" << std::endl;
  return false;
}

}  // namespace qrb_camera
