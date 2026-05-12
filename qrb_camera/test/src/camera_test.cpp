/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All Rights Reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
/*
 * QCarCam standalone test – no ROS node required.
 *
 * Build:  colcon build --packages-select qrb_camera \
 *                      --cmake-args -DUSE_QCARCAM=ON -DBUILD_TESTING=ON
 * Run:    ./install/qrb_camera/lib/qrb_camera/qrb_camera_test_qcarcam
 *
 * The test opens camera CAMERA_ID with one NV12 stream, prints a summary line
 * every 30 frames, then stops cleanly after RUN_SECONDS (or on Ctrl-C).
 *
 * Internal call order (matches QCarCamCamera::start_camera):
 *   QCarCamInitialize
 *   QCarCamQueryInputs  (verify input present)
 *   QCarCamOpen         (REQUEST_MODE)
 *   QCarCamSetParam     (ISP_USECASE, EVENT_MASK)
 *   QCarCamRegisterEventCallback
 *   QCarCamSetBuffers   (per stream, GBM-backed)
 *   QCarCamReserve
 *   QCarCamStart
 *   QCarCamSubmitRequest (initial pre-queue)
 *   ... FRAME_READY events → on_preview_frame → image_cb ...
 *   QCarCamStop → QCarCamRelease → QCarCamClose → QCarCamUninitialize
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <jpeglib.h>

#include "qrb_camera/camera_manager.hpp"
#include "qrb_camera/qrb_camera_common.hpp"

using namespace qrb_camera;

// ─── Adjust to match your board / use case ────────────────────────────────
// IMX728
static constexpr uint32_t CAMERA_ID   = 11;     // QCarCam inputId (check QCarCamQueryInputs)
static constexpr uint32_t IMG_WIDTH   = 3840;
static constexpr uint32_t IMG_HEIGHT  = 2160;
static constexpr uint32_t IMG_FPS     = 30;
static constexpr int      RUN_SECONDS = 10;
static constexpr uint32_t SAVE_FRAMES = 20;
static constexpr const char * SAVE_DIR = "/tmp/";

// // OX03F10
// static constexpr uint32_t CAMERA_ID   = 6;          // QCarCam inputId (check QCarCamQueryInputs)
// static constexpr uint32_t IMG_WIDTH   = 1824;
// static constexpr uint32_t IMG_HEIGHT  = 1536;
// static constexpr uint32_t IMG_FPS     = 30;
// static constexpr int      RUN_SECONDS = 10;
// static constexpr uint32_t SAVE_FRAMES = 3;
// static constexpr const char * SAVE_DIR = "/tmp/";
// ──────────────────────────────────────────────────────────────────────────

static std::atomic<bool> g_running{true};

// NV12 (semi-planar YUV 4:2:0) → packed RGB24
static void nv12_to_rgb(const uint8_t * src, uint8_t * dst,
                         uint32_t w, uint32_t h, uint32_t stride)
{
  const uint8_t * y_plane  = src;
  const uint8_t * uv_plane = src + static_cast<size_t>(stride) * h;
  for (uint32_t row = 0; row < h; ++row) {
    for (uint32_t col = 0; col < w; ++col) {
      int Y = y_plane[row * stride + col];
      int U = uv_plane[(row >> 1) * stride + (col & ~1u)];
      int V = uv_plane[(row >> 1) * stride + (col & ~1u) + 1];
      int c = Y - 16, d = U - 128, e = V - 128;
      uint8_t * p = dst + (row * w + col) * 3;
      p[0] = (uint8_t)std::clamp((298 * c + 409 * e + 128) >> 8, 0, 255);
      p[1] = (uint8_t)std::clamp((298 * c - 100 * d - 208 * e + 128) >> 8, 0, 255);
      p[2] = (uint8_t)std::clamp((298 * c + 516 * d + 128) >> 8, 0, 255);
    }
  }
}

static bool save_jpeg(const char * path, const uint8_t * rgb, int w, int h, int quality)
{
  FILE * f = fopen(path, "wb");
  if (!f) return false;

  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  jpeg_stdio_dest(&cinfo, f);

  cinfo.image_width      = w;
  cinfo.image_height     = h;
  cinfo.input_components = 3;
  cinfo.in_color_space   = JCS_RGB;
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, quality, TRUE);
  jpeg_start_compress(&cinfo, TRUE);

  while (cinfo.next_scanline < cinfo.image_height) {
    JSAMPROW row = const_cast<JSAMPROW>(rgb + cinfo.next_scanline * w * 3);
    JSAMPROW row_pointer[1];
    row_pointer[0] = row;
    jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);
  fclose(f);
  return true;
}

static void signal_handler(int)
{
  g_running = false;
}

int main()
{
  std::signal(SIGINT,  signal_handler);
  std::signal(SIGTERM, signal_handler);

  CameraManager manager;

  // ── 1. Create device ──────────────────────────────────────────────────
  std::cout << "###debug...start to create_camera" << std::endl;
  auto index = manager.create_camera(CameraType::QCarCam, CAMERA_ID);
  std::cout << "###debug...end to create_camera" << std::endl;
  std::cout << "###debug...succss to create camera, index: " << index << std::endl;
  if (index < 0) {
    std::cerr << "[ERROR] create_camera failed (inputId=" << CAMERA_ID << ")" << std::endl;
    return -1;
  }

  // ── 2. Configure stream ───────────────────────────────────────────────
  // Supported formats: "nv12", "nv21", "uyvy"
  // stream_type MUST be pushed for every entry in stream_size,
  // otherwise CameraManager::convert_detail_to_param() will access
  // stream_type[] out-of-bounds and crash.
  CameraConfigure detail;
  detail.camera_id   = static_cast<int>(CAMERA_ID);
  detail.camera_type = "single";
  detail.stream_size = 1;

  detail.stream_names.push_back("main");
  detail.stream_formats.push_back("nv12");
  detail.stream_widths.push_back(IMG_WIDTH);
  detail.stream_heights.push_back(IMG_HEIGHT);
  detail.stream_fps.push_back(IMG_FPS);
  detail.stream_type.push_back(StreamType::Track);  // required, must match stream_size

  std::cout << "###debug...start to set_camera_parameter" << std::endl;
  if (!manager.set_camera_parameter(index, detail)) {
    std::cerr << "[ERROR] set_camera_parameter failed" << std::endl;
    return -1;
  }
  std::cout << "###debug...end to set_camera_parameter" << std::endl;

  // ── 3. Register callback BEFORE start_camera ─────────────────────────
  // QCarCamCamera::on_frame_ready() snapshots listeners_ inside the driver
  // callback thread.  Registering after start_camera risks dropping the
  // first frames (listeners_ empty → immediate resubmit without dispatch).
  std::atomic<uint64_t> frame_count{0};
  std::atomic<uint32_t> save_count{0};

  auto image_cb = [&frame_count, &save_count](std::unique_ptr<CameraFrame> frame) {
    uint64_t n = ++frame_count;

    if (/*n > 3 && */n < 20 && frame->data) {
        printf("[DATA] frame %lu first 16 bytes:", n);
        for (int i = 0; i < 16; i++) {
            printf(" %02X", frame->data[i]);
        }
        printf("\n");
    }

    // if (n % 30 == 0) {
      std::cout << "[FRAME]"
                << "  stream="  << frame->stream_name
                << "  id="      << frame->frame_id
                << "  res="     << frame->width << "x" << frame->height
                << "  fmt="     << frame->format
                << "  stride="  << frame->stride
                << "  fd="      << frame->fd
                << "  ts_ns="   << frame->timestamp
                << "  total="   << n
                << std::endl;
    // }

    // uint32_t idx = save_count.fetch_add(1);
    
    if (/*n > 3 &&*/ n < 20 && frame->data) {
      uint32_t idx = save_count.load();
      std::cout << "###debug...idx: " << idx << std::endl;
      if (idx < SAVE_FRAMES) {
        save_count.fetch_add(1);
        std::vector<uint8_t> rgb(frame->width * frame->height * 3);
        nv12_to_rgb(frame->data, rgb.data(), frame->width, frame->height, frame->stride);
        std::string path = std::string(SAVE_DIR) + "frame_" + std::to_string(idx + 1) + ".jpg";
        if (save_jpeg(path.c_str(), rgb.data(), (int)frame->width, (int)frame->height, 90)) {
            std::cout << "[SAVE] " << path << std::endl;
        }
      }
    }
  };

  std::cout << "###debug...start to register_callback" << std::endl;
  if (!manager.register_callback(index, image_cb)) {
    std::cerr << "[ERROR] register_callback failed" << std::endl;
    return -1;
  }
  std::cout << "###debug...end to register_callback" << std::endl;

  // ── 4. Start camera ───────────────────────────────────────────────────
  // Internally: QCarCamInitialize → QueryInputs → Open (REQUEST_MODE) →
  //   SetParam(ISP_USECASE) → RegisterEventCallback → SetParam(EVENT_MASK) →
  //   allocate_buffers (GBM) → QCarCamSetBuffers → QCarCamReserve →
  //   QCarCamStart → submit_initial (pre-queue all buffer slots)
  std::cout << "###debug...start to start_camera" << std::endl;
  if (!manager.start_camera(index)) {
    std::cerr << "[ERROR] start_camera failed" << std::endl;
    return -1;
  }
  std::cout << "###debug...end to start_camera" << std::endl;

  std::cout << "[INFO] Camera " << CAMERA_ID << " started ("
            << IMG_WIDTH << "x" << IMG_HEIGHT << " @" << IMG_FPS << "fps nv12)."
            << "  Running for up to " << RUN_SECONDS << "s – Ctrl-C to stop early."
            << std::endl;

  // ── 5. Run until deadline or signal ──────────────────────────────────
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(RUN_SECONDS);
  while (g_running && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // ── 6. Stop cleanly ───────────────────────────────────────────────────
  // Internally: QCarCamStop → QCarCamRelease → QCarCamClose →
  //   QCarCamUninitialize → free_buffers (munmap + gbm_bo_destroy) →
  //   gbm_device_destroy → close(drm_fd)
  std::cout << "###debug...start to stop_camera" << std::endl;
  manager.stop_camera(index);
  std::cout << "###debug...end to stop_camera" << std::endl;

  std::cout << "[INFO] Stopped. Total frames received: " << frame_count.load() << std::endl;
  return 0;
}
