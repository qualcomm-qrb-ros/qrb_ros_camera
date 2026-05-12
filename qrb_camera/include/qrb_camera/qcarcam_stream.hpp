/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#ifndef QRB_CAMERA__QCARCAM_STREAM_HPP_
#define QRB_CAMERA__QCARCAM_STREAM_HPP_

// =============================================================================
// QCarCamStream – per-stream state for the QCarCam submit-request backend.
//
// Buffers are allocated via Linux DMA heap (/dev/dma_heap/system) — no
// GBM/DRM dependency required.  Each buffer is a contiguous dma-buf covering
// all YUV planes; planes[0].memHndl carries the fd (matching qcc.cpp).
//
// QCarCam multi-stream model:
//   One QCarCamOpen handle can carry multiple QCarCamBufferList_t objects,
//   each identified by a unique bufferlist id assigned by QCarCamSetBuffers.
//   A FRAME_READY event carries payload->frameInfo.id to identify which
//   bufferlist (i.e. which stream) the frame belongs to.
//
// Lifecycle (called by QCarCamCamera):
//   init()              – store stream parameters
//   allocate_buffers()  – DMA-heap alloc + fill QCarCamBufferList_t
//   submit_initial()    – pre-queue all buffers after QCarCamStart
//   on_frame_ready()    – called by QCarCamCamera when frameInfo.id matches
//   free_buffers()      – unmap + close dma-buf fds
// =============================================================================

#include <qcarcam.h>

#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>
#include <string>
#include <vector>

#include "qrb_camera/camera_interface.hpp"
#include "qrb_camera/qrb_camera_common.hpp"

namespace qrb_camera
{

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr uint32_t QCARCAM_NUM_BUFFERS        = 5;
static constexpr uint32_t QCARCAM_MAX_IN_USE_BUFFERS = 4;

// ---------------------------------------------------------------------------
// DMA-heap buffer slot (one per output buffer per stream)
// ---------------------------------------------------------------------------
struct DmaHeapBuf
{
  int    fd    = -1;
  size_t len   = 0;
  void * vaddr = nullptr;
};

// ---------------------------------------------------------------------------
// ResubmitFn – callback provided by QCarCamCamera so QCarCamStream can
// call QCarCamSubmitRequest without knowing the handle or request object.
// Signature: resubmit_fn(bufferlist_id, buf_idx)
// ---------------------------------------------------------------------------
using ResubmitFn = std::function<void(uint32_t bufferlist_id, uint32_t buf_idx)>;

// ---------------------------------------------------------------------------
// QCarCamStream
// ---------------------------------------------------------------------------
class QCarCamStream
{
public:
  QCarCamStream() = default;
  ~QCarCamStream() = default;

  QCarCamStream(const QCarCamStream &)            = delete;
  QCarCamStream & operator=(const QCarCamStream &) = delete;
  QCarCamStream(QCarCamStream &&)                  = default;
  QCarCamStream & operator=(QCarCamStream &&)      = default;

  // ---- Setup (called before QCarCamSetBuffers) ----

  bool init(const StreamParam & sp,
            uint32_t            camera_id,
            ResubmitFn          resubmit_fn);

  // Allocate DMA-heap buffers and fill buf_list_.
  bool allocate_buffers();

  // Pre-queue all buffers after QCarCamStart.
  bool submit_initial();

  // Release all DMA-heap resources.
  void free_buffers();

  // ---- Runtime (called from QCarCamCamera event callback) ----

  void on_frame_ready(uint32_t                               buf_idx,
                      const std::vector<ICameraListener *> & listeners);

  // ---- Accessors ----

  QCarCamBufferList_t & buf_list() { return buf_list_; }
  uint32_t buf_list_id() const { return buf_list_.id; }
  const std::string & stream_name() const { return stream_name_; }

private:
  // ---- Format helper ----
  static std::string to_ros_encoding(QCarCamColorFmt_e fmt);

  // ---- Per-frame helpers ----
  void dispatch_frame(uint32_t                               buf_idx,
                      const std::vector<ICameraListener *> & listeners);
  void drain_release_queue();

  // ---- Stream parameters ----
  uint32_t          width_       = 1920;
  uint32_t          height_      = 1080;
  uint32_t          fps_         = 30;
  uint32_t          camera_id_   = 0;
  std::string       stream_name_ = "stream";
  QCarCamColorFmt_e color_fmt_   = QCARCAM_FMT_NV12;

  // ---- QCarCam buffer descriptors ----
  // buf_list_.pBuffers points into buffers_[].
  QCarCamBufferList_t buf_list_                     = {};
  QCarCamBuffer_t     buffers_[QCARCAM_NUM_BUFFERS] = {};

  // ---- DMA-heap buffer slots ----
  DmaHeapBuf dma_bufs_[QCARCAM_NUM_BUFFERS] = {};

  // ---- Buffer queue management ----
  std::mutex          buf_mtx_;
  std::list<uint32_t> avail_queue_;
  std::list<uint32_t> release_queue_;
  uint32_t            occupied_cnt_ = 0;

  // ---- Resubmit callback (provided by QCarCamCamera) ----
  ResubmitFn resubmit_fn_;

  // ---- Frame counter ----
  uint32_t frame_cnt_ = 0;

  std::string logger_ = "QCarCamStream";
};

}  // namespace qrb_camera
#endif  // QRB_CAMERA__QCARCAM_STREAM_HPP_
