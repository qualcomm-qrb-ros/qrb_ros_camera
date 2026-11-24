/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef CAMERA_BACKEND_QCARCAM__QCARCAM_CAMERA_HPP_
#define CAMERA_BACKEND_QCARCAM__QCARCAM_CAMERA_HPP_

#include <qcarcam.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "camera_interface/camera_interface.hpp"
#include "qcarcam_stream.hpp"

namespace qrb_camera
{
static constexpr uint32_t QCARCAM_MAX_OPEN_RETRY = 100U;
static constexpr uint32_t QCARCAM_MAX_QUERY_RETRY = 500U;
static constexpr uint32_t QCARCAM_SENSOR_WAIT_US = 10000U;

// ISP usecase 138: full ISP pipeline including IPE (Image Processing Engine)
static constexpr uint32_t QCARCAM_ISP_USECASE = 138U;
// Buffer list type 5: NV12 output format
static constexpr uint32_t QCARCAM_BUFFERLIST_TYPE = 5U;

// ---------------------------------------------------------------------------
// QCarCamCamera – one QCarCam input in submit-request mode, one or more streams.
//
// The SDK is initialized by QCarCamBackend (QCarCamInitialize in probe(),
// QCarCamUninitialize when the backend is destroyed, after every camera), so this class
// never calls QCarCamInitialize / QCarCamUninitialize itself.
//
// Call order (per qcarcam_test.cpp):
//   init()            checks that stream parameters were set
//   start_camera()
//     QCarCamQueryInputs  (BEFORE Open: two-step, count then fill; retried)
//     QCarCamOpen         (with QCARCAM_OPEN_FLAGS_REQUEST_MODE; retried)
//     QCarCamSetParam     (ISP_USECASE)
//     QCarCamRegisterEventCallback
//     QCarCamSetParam     (EVENT_MASK)
//     for each stream: allocate DMA-heap buffers, QCarCamSetBuffers
//     QCarCamReserve      (after ALL SetBuffers, before Start)
//     QCarCamStart
//     for each stream: submit_initial()
//   [event callback] QCARCAM_EVENT_FRAME_READY -> QCarCamStream::on_frame_ready()
//   stop_camera()     QCarCamStop -> QCarCamRelease -> QCarCamClose, free the buffers
// Frame timestamps are QTimer ns (see now_ns()).
// ---------------------------------------------------------------------------
class QCarCamCamera : public ICameraDevice
{
public:
  explicit QCarCamCamera(uint32_t camera_id);
  ~QCarCamCamera() override;

  void set_param(std::vector<CameraParam> & param) override;
  bool init() override;
  int start_camera() override;
  int stop_camera() override;
  void add_listener(ICameraListener * listener) override;
  void remove_listener(ICameraListener * listener) override;
  int64_t now_ns() const override;

  // Called by the static QCarCam event callback
  void on_frame_ready(const QCarCamEventPayload_t * payload);
  void on_error(const QCarCamEventPayload_t * payload);

  // Submit one buffer back to the driver; called by the streams. Serialized by
  // request_mtx_. requestId is ALWAYS incremented, success or not (as qcarcam_test.cpp).
  void resubmit_buffer(uint32_t bufferlist_id, uint32_t buf_idx);

private:
  bool query_and_verify_input();

  QCarCamHndl_t handle_ = QCARCAM_HNDL_INVALID;
  QCarCamIspUsecaseConfig_t isp_config_ = {};
  // request_ is reused for every QCarCamSubmitRequest call.
  QCarCamRequest_t request_ = {};
  std::mutex request_mtx_;

  std::vector<StreamParam> stream_params_;
  // Heap-allocated: each stream's buffer list points into the stream object itself.
  std::vector<std::unique_ptr<QCarCamStream>> streams_;

  std::mutex listener_mtx_;
  std::vector<ICameraListener *> listeners_;

  std::atomic<bool> running_{false};

  std::string logger_ = "QCarCamCamera";
};
}  // namespace qrb_camera

#endif  // CAMERA_BACKEND_QCARCAM__QCARCAM_CAMERA_HPP_
