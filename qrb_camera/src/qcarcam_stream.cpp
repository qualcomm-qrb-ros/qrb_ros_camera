/*
 *​​​​ Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

// =============================================================================
// QCarCamStream – per-stream DMA-heap buffer management and frame dispatch.
//
// Buffers are allocated via /dev/dma_heap/system (Linux DMA heap interface).
// Each buffer is a single dma-buf fd covering all YUV planes contiguously.
// planes[0].memHndl = dma-buf fd  (only plane 0, matching qcc.cpp convention).
//
// Buffer lifecycle per frame:
//   allocate_buffers()  → QCARCAM_NUM_BUFFERS dma-buf fds allocated + mmap'd
//   submit_initial()    → buffer is "owned by driver"
//   FRAME_READY event   → driver→avail_queue_
//   dispatch_frame()    → avail_queue_→occupied_cnt_ (listener holds it)
//   listener returns    → occupied_cnt_→release_queue_
//   drain_release_queue → release_queue_→driver (via resubmit_fn_)
// =============================================================================

#include "qrb_camera/qcarcam_stream.hpp"
#include "qrb_camera/qcarcam_frame.hpp"

#include <fcntl.h>
#include <linux/dma-heap.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <iostream>

// ---------------------------------------------------------------------------
// open_dma_heap – open the first usable DMA heap device node.
// Returns an open fd, or -1 on failure.
// ---------------------------------------------------------------------------
static int open_dma_heap()
{
  static const char * heaps[] = {
    "/dev/dma_heap/qcom,system",
    "/dev/dma_heap/system",
    "/dev/dma_heap/linux,cma",
    nullptr
  };
  for (const char ** h = heaps; *h; ++h) {
    int fd = open(*h, O_RDONLY | O_CLOEXEC);
    if (fd >= 0) {
      std::cout << "[INFO] [QCarCamStream]: using DMA heap " << *h << std::endl;
      return fd;
    }
  }
  return -1;
}

namespace qrb_camera
{

// ---------------------------------------------------------------------------
// compute_plane_layout – stride / size for each YUV plane without GBM.
//
// Qualcomm alignment convention (SA8775P / IQ10):
//   stride     : round up to nearest 128 bytes  (QCARCAM_STRIDE_ALIGN)
//   height     : round up to nearest 32 lines   (QCARCAM_HEIGHT_ALIGN)
//
// Only NV12 / NV21 (2-plane) and packed UYVY (1-plane) are handled;
// other formats fall back to NV12 layout.
// ---------------------------------------------------------------------------
static void compute_plane_layout(QCarCamColorFmt_e fmt,
                                  uint32_t width, uint32_t height,
                                  uint32_t & stride,
                                  uint32_t & total_size,
                                  uint32_t & num_planes,
                                  uint32_t   plane_sizes[3])
{
  uint32_t h_align = (height + QCARCAM_HEIGHT_ALIGN - 1u) & ~(QCARCAM_HEIGHT_ALIGN - 1u);

  switch (fmt) {
    case QCARCAM_FMT_NV12:
    case QCARCAM_FMT_NV21: {
      stride         = (width + QCARCAM_STRIDE_ALIGN - 1u) & ~(QCARCAM_STRIDE_ALIGN - 1u);
      plane_sizes[0] = stride * h_align;           // Y
      plane_sizes[1] = stride * (h_align / 2u);    // UV interleaved
      total_size     = plane_sizes[0] + plane_sizes[1];
      num_planes     = 2;
      break;
    }
    case QCARCAM_FMT_UYVY_8: {
      stride         = ((width * 2u) + QCARCAM_STRIDE_ALIGN - 1u) & ~(QCARCAM_STRIDE_ALIGN - 1u);
      plane_sizes[0] = stride * h_align;
      total_size     = plane_sizes[0];
      num_planes     = 1;
      break;
    }
    default: {
      // Fallback: treat as NV12
      stride         = (width + QCARCAM_STRIDE_ALIGN - 1u) & ~(QCARCAM_STRIDE_ALIGN - 1u);
      plane_sizes[0] = stride * h_align;
      plane_sizes[1] = stride * (h_align / 2u);
      total_size     = plane_sizes[0] + plane_sizes[1];
      num_planes     = 2;
      break;
    }
  }
}

// ---------------------------------------------------------------------------
// init – store stream parameters
// ---------------------------------------------------------------------------
bool QCarCamStream::init(const StreamParam & sp,
                          uint32_t            camera_id,
                          ResubmitFn          resubmit_fn)
{
  width_       = sp.width;
  height_      = sp.height;
  fps_         = sp.frame_rate;
  stream_name_ = sp.name;
  camera_id_   = camera_id;
  resubmit_fn_ = resubmit_fn;

  if (sp.format == "nv12" || sp.format == "NV12") {
    color_fmt_ = QCARCAM_FMT_NV12;
  } else if (sp.format == "nv21" || sp.format == "NV21") {
    color_fmt_ = QCARCAM_FMT_NV21;
  } else if (sp.format == "uyvy" || sp.format == "UYVY") {
    color_fmt_ = QCARCAM_FMT_UYVY_8;
  } else {
    std::cout << "[WARN] [" << logger_ << "]: unknown format '" << sp.format
              << "', defaulting to NV12" << std::endl;
    color_fmt_ = QCARCAM_FMT_NV12;
  }

  std::cout << "[INFO] [" << logger_ << "]: init stream '" << stream_name_
            << "' " << width_ << "x" << height_ << " @" << fps_ << "fps"
            << " fmt=" << sp.format << std::endl;
  return true;
}

// ---------------------------------------------------------------------------
// allocate_buffers – DMA-heap alloc + fill QCarCamBufferList_t
//
// NOTE: buf_list_.pBuffers = buffers_ stores a pointer into this object's
// member array.  The caller (QCarCamCamera) must not allow vector reallocation
// after this call, since that would invalidate the pointer.
// ---------------------------------------------------------------------------
bool QCarCamStream::allocate_buffers()
{
  uint32_t stride     = 0;
  uint32_t total_size = 0;
  uint32_t num_planes = 0;
  uint32_t plane_sizes[3] = {};
  compute_plane_layout(color_fmt_, width_, height_,
                       stride, total_size, num_planes, plane_sizes);

  if (stride == 0 || total_size == 0) {
    std::cout << "[ERROR] [" << logger_ << "]: unsupported format or zero stride" << std::endl;
    return false;
  }

  int heap_fd = open_dma_heap();
  if (heap_fd < 0) {
    std::cout << "[ERROR] [" << logger_ << "]: no DMA heap found (errno=" << errno << ")"
              << std::endl;
    return false;
  }

  for (uint32_t i = 0; i < QCARCAM_NUM_BUFFERS; ++i) {
    struct dma_heap_allocation_data alloc = {};
    alloc.len      = total_size;
    alloc.fd_flags = O_RDWR | O_CLOEXEC;

    if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc) < 0) {
      std::cout << "[ERROR] [" << logger_ << "]: DMA_HEAP_IOCTL_ALLOC failed buf=" << i
                << " errno=" << errno << std::endl;
      close(heap_fd);
      free_buffers();
      return false;
    }

    int    buf_fd = static_cast<int>(alloc.fd);
    void * vaddr  = mmap(nullptr, total_size,
                         PROT_READ | PROT_WRITE, MAP_SHARED, buf_fd, 0);
    if (vaddr == MAP_FAILED) {
      vaddr = nullptr;
    }

    dma_bufs_[i].fd    = buf_fd;
    dma_bufs_[i].len   = total_size;
    dma_bufs_[i].vaddr = vaddr;

    // Plane 0 – carries the dma-buf fd (planes[0].memHndl only, per qcc.cpp)
    buffers_[i].numPlanes         = num_planes;
    buffers_[i].planes[0].width   = width_;
    buffers_[i].planes[0].height  = height_;
    buffers_[i].planes[0].stride  = stride;
    buffers_[i].planes[0].size    = plane_sizes[0];
    buffers_[i].planes[0].memHndl = static_cast<uint64_t>(buf_fd);

    // Plane 1 (NV12 / NV21)
    if (num_planes >= 2) {
      buffers_[i].planes[1].stride = stride;
      buffers_[i].planes[1].size   = plane_sizes[1];
      // planes[1].memHndl intentionally left 0 – driver infers from plane 0 fd
    }
  }

  close(heap_fd);  // heap_fd not needed after allocation

  buf_list_.nBuffers = QCARCAM_NUM_BUFFERS;
  buf_list_.colorFmt = color_fmt_;
  buf_list_.pBuffers = buffers_;

  std::cout << "[INFO] [" << logger_ << "]: allocated " << QCARCAM_NUM_BUFFERS
            << " DMA-heap buffers for stream '" << stream_name_
            << "' " << width_ << "x" << height_
            << " stride=" << stride << " total=" << total_size << "B" << std::endl;
  return true;
}

// ---------------------------------------------------------------------------
// submit_initial – pre-queue all buffer slots after QCarCamStart
// ---------------------------------------------------------------------------
bool QCarCamStream::submit_initial()
{
  for (uint32_t i = 0; i < QCARCAM_NUM_BUFFERS; ++i) {
    resubmit_fn_(buf_list_.id, i);
  }
  std::cout << "[INFO] [" << logger_ << "]: submitted " << QCARCAM_NUM_BUFFERS
            << " initial requests for stream '" << stream_name_ << "'" << std::endl;
  return true;
}

// ---------------------------------------------------------------------------
// free_buffers – release all DMA-heap resources
// ---------------------------------------------------------------------------
void QCarCamStream::free_buffers()
{
  for (uint32_t i = 0; i < QCARCAM_NUM_BUFFERS; ++i) {
    if (dma_bufs_[i].vaddr && dma_bufs_[i].vaddr != MAP_FAILED) {
      munmap(dma_bufs_[i].vaddr, dma_bufs_[i].len);
      dma_bufs_[i].vaddr = nullptr;
    }
    if (dma_bufs_[i].fd >= 0) {
      close(dma_bufs_[i].fd);
      dma_bufs_[i].fd = -1;
    }
    dma_bufs_[i].len = 0;
  }
  buf_list_.pBuffers = nullptr;
}

// ---------------------------------------------------------------------------
// on_frame_ready – entry point from QCarCamCamera event callback
// ---------------------------------------------------------------------------
void QCarCamStream::on_frame_ready(uint32_t                               buf_idx,
                                    uint64_t                               frame_timestamp,
                                    const std::vector<ICameraListener *> & listeners)
{
  if (buf_idx >= QCARCAM_NUM_BUFFERS) {
    std::cout << "[ERROR] [" << logger_ << "]: invalid buf_idx=" << buf_idx << std::endl;
    return;
  }

  frame_cnt_++;

  std::list<uint32_t> to_drop;
  {
    std::lock_guard<std::mutex> lk(buf_mtx_);
    while (avail_queue_.size() + occupied_cnt_ >= QCARCAM_MAX_IN_USE_BUFFERS) {
      uint32_t drop = avail_queue_.front();
      avail_queue_.pop_front();
      to_drop.push_back(drop);
      std::cout << "[WARN] [" << logger_ << "]: stream '" << stream_name_
                << "' dropped oldest frame buf=" << drop << " (queue full)" << std::endl;
    }
    avail_queue_.push_back(buf_idx);
  }

  for (uint32_t drop_idx : to_drop) {
    resubmit_fn_(buf_list_.id, drop_idx);
  }

  dispatch_frame(buf_idx, frame_timestamp, listeners);
  drain_release_queue();
}

// ---------------------------------------------------------------------------
// dispatch_frame – build ICameraFrame and call on_preview_frame synchronously
// ---------------------------------------------------------------------------
void QCarCamStream::dispatch_frame(uint32_t                               buf_idx,
                                    uint64_t                               frame_timestamp,
                                    const std::vector<ICameraListener *> & listeners)
{
  if (listeners.empty()) {
    std::lock_guard<std::mutex> lk(buf_mtx_);
    avail_queue_.remove(buf_idx);
    release_queue_.push_back(buf_idx);
    return;
  }

  QCarCamFrame frame;
  frame.fd           = dma_bufs_[buf_idx].fd;
  frame.data         = static_cast<uint8_t *>(dma_bufs_[buf_idx].vaddr);
  frame.size         = static_cast<uint32_t>(dma_bufs_[buf_idx].len);
  frame.width        = width_;
  frame.height       = height_;
  frame.stride       = buffers_[buf_idx].planes[0].stride;
  frame.slice        = (height_ + QCARCAM_HEIGHT_ALIGN - 1u) & ~(QCARCAM_HEIGHT_ALIGN - 1u);
  frame.format       = to_ros_encoding(color_fmt_);
  frame.frame_source = stream_name_;
  frame.index        = frame_cnt_;
  frame.camera_id    = camera_id_;
  frame.orientation  = 0;
  frame.metadata     = nullptr;

  frame.timestamp = frame_timestamp;

  {
    std::lock_guard<std::mutex> lk(buf_mtx_);
    avail_queue_.remove(buf_idx);
    ++occupied_cnt_;
  }

  for (auto * listener : listeners) {
    listener->on_preview_frame(static_cast<ICameraFrame *>(&frame));
  }

  {
    std::lock_guard<std::mutex> lk(buf_mtx_);
    --occupied_cnt_;
    release_queue_.push_back(buf_idx);
  }
}

// ---------------------------------------------------------------------------
// drain_release_queue – re-submit all buffers returned by listeners
// ---------------------------------------------------------------------------
void QCarCamStream::drain_release_queue()
{
  std::list<uint32_t> to_release;
  {
    std::lock_guard<std::mutex> lk(buf_mtx_);
    to_release.swap(release_queue_);
  }
  for (uint32_t rel_idx : to_release) {
    resubmit_fn_(buf_list_.id, rel_idx);
  }
}

// ---------------------------------------------------------------------------
// Format helper
// ---------------------------------------------------------------------------
std::string QCarCamStream::to_ros_encoding(QCarCamColorFmt_e fmt)
{
  switch (fmt) {
    case QCARCAM_FMT_NV12:   return "nv12";
    case QCARCAM_FMT_NV21:   return "nv21";
    case QCARCAM_FMT_UYVY_8: return "uyvy";
    default:                 return "nv12";
  }
}

}  // namespace qrb_camera
