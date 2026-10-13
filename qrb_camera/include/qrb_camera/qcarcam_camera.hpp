/*
 *​​​​ Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef QRB_CAMERA__QCARCAM_CAMERA_HPP_
#define QRB_CAMERA__QCARCAM_CAMERA_HPP_

#include <qcarcam.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "qrb_camera/camera_interface.hpp"
#include "qrb_camera/qcarcam_stream.hpp"

namespace qrb_camera
{

// ---------------------------------------------------------------------------
// Constants (stream constants are in qcarcam_stream.hpp)
// ---------------------------------------------------------------------------
static constexpr uint32_t QCARCAM_MAX_OPEN_RETRY = 100U;
static constexpr uint32_t QCARCAM_MAX_QUERY_RETRY = 500U;
static constexpr uint32_t QCARCAM_SENSOR_WAIT_US = 10000U;
static constexpr const char * QCARCAM_QCX_MQUEUE_PATH = "/dev/mqueue/QCX_debfcadf";

// ---------------------------------------------------------------------------
// QCarCamCamera – ICameraDevice implementation using QCarCam submit-request
// mode.  Supports multiple simultaneous streams from a single camera input.
//
// Correct API call order (per qcarcam_test.cpp):
//   init()
//     WaitForQCX -> QCarCamInitialize
//   start_camera()
//     QCarCamQueryInputs  (BEFORE Open: two-step, count then fill)
//     QCarCamOpen         (with QCARCAM_OPEN_FLAGS_REQUEST_MODE)
//     QCarCamSetParam     (ISP_USECASE)
//     QCarCamRegisterEventCallback
//     QCarCamSetParam     (EVENT_MASK)
//     for each stream:
//       QCarCamStream::allocate_buffers()  (DMA-heap, no GBM/DRM)
//       QCarCamSetBuffers
//     QCarCamReserve      (after ALL SetBuffers, before Start)
//     QCarCamStart
//     for each stream: QCarCamStream::submit_initial()
//   [event callback]
//     QCARCAM_EVENT_FRAME_READY
//       route by payload->frameInfo.id to matching QCarCamStream
//       QCarCamStream::on_frame_ready() manages queues + dispatches listeners
//       resubmit_buffer() called via ResubmitFn (always increments requestId)
//   stop_camera()
//     QCarCamStop -> QCarCamRelease -> QCarCamClose -> QCarCamUninitialize
//     for each stream: free_buffers()
// ---------------------------------------------------------------------------
class QCarCamCamera : public ICameraDevice
{
public:
  explicit QCarCamCamera(uint32_t camera_id);
  ~QCarCamCamera() override;

  // ICameraDevice interface
  void set_param(std::vector<CameraParam> & param) override;
  bool init() override;
  int start_camera() override;
  int stop_camera() override;
  void add_listener(ICameraListener * listener) override;
  void remove_listener(ICameraListener * listener) override;

  // Called by the static QCarCam event callback
  void on_frame_ready(const QCarCamEventPayload_t * payload);
  void on_error(const QCarCamEventPayload_t * payload);

  // Called by QCarCamStream objects (via ResubmitFn lambda) to resubmit a
  // buffer back to the driver.  Thread-safe (protected by request_mtx_).
  // requestId is ALWAYS incremented regardless of success to avoid duplicate
  // IDs (matches qcarcam_test.cpp behaviour).
  void resubmit_buffer(uint32_t bufferlist_id, uint32_t buf_idx);

private:
  // ---- Initialisation helpers ----
  bool wait_for_qcx();
  bool query_and_verify_input();

  // ---- QCarCam handle / config ----
  QCarCamHndl_t handle_ = QCARCAM_HNDL_INVALID;
  QCarCamIspUsecaseConfig_t isp_config_ = {};
  // request_ is reused for every QCarCamSubmitRequest call.
  // request_mtx_ serialises concurrent resubmits from multiple streams.
  QCarCamRequest_t request_ = {};
  std::mutex request_mtx_;

  // ---- Per-stream state ----
  std::vector<StreamParam> stream_params_;
  std::vector<std::unique_ptr<QCarCamStream>> streams_;

  // ---- Listener list (shared across all streams) ----
  std::mutex listener_mtx_;
  std::vector<ICameraListener *> listeners_;

  // ---- Lifecycle ----
  std::atomic<bool> running_{false};
  bool is_initialized_ = false;

  std::string logger_ = "QCarCamCamera";
};

}  // namespace qrb_camera
#endif  // QRB_CAMERA__QCARCAM_CAMERA_HPP_
