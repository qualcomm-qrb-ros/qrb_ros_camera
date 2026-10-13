/*
 *​​​​ Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
/*
 * QCarCam standalone test – no ROS node required.
 *
 * Reads three simultaneous streams from a single D457 input as configured in
 * config_d457.xml:
 *
 *   Stream 0  context_id=0  bufferlist_id=1  1280×720  mipi16  (depth / IR)
 *   Stream 1  context_id=1  bufferlist_id=2  1280×720  mipi16  (depth / IR)
 *   Stream 2  context_id=2  bufferlist_id=3  1280×720  mipi8   (depth / IR)
 *
 * Build:
 *   colcon build --packages-select qrb_camera \
 *                --cmake-args -DUSE_QCARCAM=ON -DBUILD_TESTING=ON
 * Run:
 *   ./install/qrb_camera/lib/qrb_camera/qrb_camera_test_qcarcam \
 *       <camera_id> <width> <height>
 *
 * The test skips the first WARMUP_SECONDS of frames (camera black-frame
 * warm-up period) and then saves SAVE_FRAMES raw files per stream.
 *
 * Internal call order (matches QCarCamCamera::start_camera):
 *   QCarCamInitialize
 *   QCarCamQueryInputs  (verify input present)
 *   QCarCamOpen         (REQUEST_MODE)
 *   QCarCamSetParam     (ISP_USECASE, EVENT_MASK)
 *   QCarCamRegisterEventCallback
 *   QCarCamSetBuffers   (per stream, DMA-heap backed)
 *   QCarCamReserve
 *   QCarCamStart
 *   QCarCamSubmitRequest (initial pre-queue)
 *   ... FRAME_READY events → on_preview_frame → image_cb ...
 *   QCarCamStop → QCarCamRelease → QCarCamClose → QCarCamUninitialize
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// jpeglib.h requires stdio.h / cstdio to be included first for FILE and size_t.
#include <jpeglib.h>

#include "qrb_camera/camera_manager.hpp"
#include "qrb_camera/qrb_camera_common.hpp"

using namespace qrb_camera;

// ─── Adjust to match your board / use case ────────────────────────────────
// D457 via config_d457.xml: qcarcam_id=4, three contexts, 1280×720
static constexpr uint32_t IMG_FPS = 30;
static constexpr int RUN_SECONDS = 10;       // total run time (warmup + dump)
static constexpr int WARMUP_SECONDS = 3;     // skip initial black frames
static constexpr uint32_t SAVE_FRAMES = 20;  // frames to dump per stream
// ──────────────────────────────────────────────────────────────────────────

static std::atomic<bool> g_running{true};

// ─── Stream descriptors matching config_d457.xml ─────────────────────────
struct StreamDesc
{
  std::string name;
  std::string format;
  uint32_t bufferlist_id;  // must match bufferlist_id in config_d457.xml
};

static const StreamDesc STREAMS[3] = {
    {"stream0", "mipi16", 1},  // Depth: RAW16
    {"stream1", "nv12", 2},    // Color: ISP demosaic output NV12
    {"stream2", "mipi8", 3},   // IR:    RAW8
};

// ─── Per-stream dump state ────────────────────────────────────────────────
struct StreamState
{
  std::atomic<uint64_t> frame_count{0};
  std::atomic<uint32_t> save_count{0};
  std::atomic<bool> warmup_done{false};
  // time_point stored as raw tick count for atomic access
  using Clock = std::chrono::steady_clock;
  std::atomic<Clock::time_point::rep> first_frame_time{0};
};

// ─── JPEG helper ─────────────────────────────────────────────────────────
static bool save_jpeg(const char * path, const uint8_t * rgb, int w, int h, int quality)
{
  FILE * f = fopen(path, "wb");
  if (!f)
    return false;

  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  jpeg_stdio_dest(&cinfo, f);

  cinfo.image_width = w;
  cinfo.image_height = h;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;
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

// ─── NV12 (semi-planar YUV 4:2:0) → packed RGB24 ─────────────────────────
static void nv12_to_rgb(const uint8_t * src, uint8_t * dst, uint32_t w, uint32_t h, uint32_t stride)
{
  const uint8_t * y_plane = src;
  const uint8_t * uv_plane = src + static_cast<size_t>(stride) * h;
  for (uint32_t row = 0; row < h; ++row) {
    for (uint32_t col = 0; col < w; ++col) {
      int Y = y_plane[row * stride + col];
      int U = uv_plane[(row >> 1) * stride + (col & ~1u)];
      int V = uv_plane[(row >> 1) * stride + (col & ~1u) + 1];
      int c = Y - 16, d = U - 128, e = V - 128;
      uint8_t * p = dst + (row * w + col) * 3;
      p[0] = static_cast<uint8_t>(std::clamp((298 * c + 409 * e + 128) >> 8, 0, 255));
      p[1] = static_cast<uint8_t>(std::clamp((298 * c - 100 * d - 208 * e + 128) >> 8, 0, 255));
      p[2] = static_cast<uint8_t>(std::clamp((298 * c + 516 * d + 128) >> 8, 0, 255));
    }
  }
}

// ─── Convert 16-bit MIPI RAW to 8-bit grey for JPEG preview ──────────────
// Each pixel is stored as a little-endian uint16; scale to [0,255].
static void
mipi16_to_grey(const uint8_t * src, uint8_t * dst, uint32_t w, uint32_t h, uint32_t stride_bytes)
{
  for (uint32_t row = 0; row < h; ++row) {
    const uint16_t * row_ptr =
        reinterpret_cast<const uint16_t *>(src + static_cast<size_t>(stride_bytes) * row);
    for (uint32_t col = 0; col < w; ++col) {
      uint16_t v = row_ptr[col];
      dst[row * w + col] = static_cast<uint8_t>(v >> 8);  // top 8 bits
    }
  }
}

// ─── Convert 8-bit MIPI RAW to grey (identity) ───────────────────────────
static void
mipi8_to_grey(const uint8_t * src, uint8_t * dst, uint32_t w, uint32_t h, uint32_t stride_bytes)
{
  for (uint32_t row = 0; row < h; ++row) {
    const uint8_t * row_ptr = src + static_cast<size_t>(stride_bytes) * row;
    for (uint32_t col = 0; col < w; ++col) {
      dst[row * w + col] = row_ptr[col];
    }
  }
}

// ─── Save a grey-scale JPEG (expand grey → RGB for libjpeg) ──────────────
static bool save_grey_jpeg(const char * path, const uint8_t * grey, int w, int h, int quality)
{
  // Expand to RGB so we can reuse the same JPEG path
  std::vector<uint8_t> rgb(static_cast<size_t>(w) * h * 3);
  for (int i = 0; i < w * h; ++i) {
    rgb[i * 3 + 0] = grey[i];
    rgb[i * 3 + 1] = grey[i];
    rgb[i * 3 + 2] = grey[i];
  }
  return save_jpeg(path, rgb.data(), w, h, quality);
}

static void signal_handler(int)
{
  g_running = false;
}

int main(int argc, char ** argv)
{
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " <camera_id> <width> <height>" << std::endl;
    return -1;
  }

  uint32_t camera_id = static_cast<uint32_t>(std::stoul(argv[1]));
  uint32_t img_width = static_cast<uint32_t>(std::stoul(argv[2]));
  uint32_t img_height = static_cast<uint32_t>(std::stoul(argv[3]));

  // Create per-stream output directories
  for (const auto & sd : STREAMS) {
    std::string dir = "./output/cam" + std::string(argv[1]) + "/" + sd.name + "/";
    std::system(("mkdir -p " + dir).c_str());
  }

  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  CameraManager manager;

  // ── 1. Create device ──────────────────────────────────────────────────
  auto index = manager.create_camera(CameraType::QCarCam, camera_id);
  if (index < 0) {
    std::cerr << "[ERROR] create_camera failed (inputId=" << camera_id << ")" << std::endl;
    return -1;
  }

  // ── 2. Configure three streams (matches config_d457.xml) ─────────────
  CameraConfigure detail;
  detail.camera_id = static_cast<int>(camera_id);
  detail.camera_type = "single";
  detail.stream_size = 3;

  for (const auto & sd : STREAMS) {
    detail.stream_names.push_back(sd.name);
    detail.stream_formats.push_back(sd.format);
    detail.stream_widths.push_back(img_width);
    detail.stream_heights.push_back(img_height);
    detail.stream_fps.push_back(IMG_FPS);
    detail.stream_type.push_back(StreamType::Track);
    detail.stream_bufferlist_ids.push_back(sd.bufferlist_id);
  }

  if (!manager.set_camera_parameter(index, detail)) {
    std::cerr << "[ERROR] set_camera_parameter failed" << std::endl;
    return -1;
  }

  // ── 3. Per-stream state + shared callback ─────────────────────────────
  // One StreamState per stream; keyed by stream_name.
  StreamState states[3];
  std::string save_base = "./output/cam" + std::string(argv[1]) + "/";

  using Clock = std::chrono::steady_clock;

  auto image_cb = [&](std::unique_ptr<CameraFrame> frame) {
    // Identify which stream this frame belongs to
    int stream_idx = -1;
    for (int i = 0; i < 3; ++i) {
      if (frame->stream_name == STREAMS[i].name) {
        stream_idx = i;
        break;
      }
    }
    if (stream_idx < 0) {
      printf("[WARN] unknown stream '%s'\n", frame->stream_name.c_str());
      return;
    }

    StreamState & st = states[stream_idx];
    uint64_t n = ++st.frame_count;

    // ── warm-up tracking ─────────────────────────────────────────────────
    if (n == 1) {
      st.first_frame_time.store(Clock::now().time_since_epoch().count(), std::memory_order_relaxed);
    }
    if (!st.warmup_done.load(std::memory_order_relaxed)) {
      auto start =
          Clock::time_point(Clock::duration(st.first_frame_time.load(std::memory_order_relaxed)));
      auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - start);
      if (elapsed.count() >= WARMUP_SECONDS) {
        st.warmup_done.store(true, std::memory_order_relaxed);
        printf("[INFO] [%s] Warm-up done (%ds). Starting dump.\n", frame->stream_name.c_str(),
            WARMUP_SECONDS);
      }
    }

    // ── metadata summary ─────────────────────────────────────────────────
    printf(
        "[META] [%s] frame %3lu  id=%u  res=%ux%u  stride=%u"
        "  fmt=%-8s  fd=%d  ts_ns=%lu\n",
        frame->stream_name.c_str(), n, frame->frame_id, frame->width, frame->height, frame->stride,
        frame->format.c_str(), frame->fd, static_cast<unsigned long>(frame->timestamp));

    // ── raw bytes: first 16 bytes ─────────────────────────────────────────
    if (frame->data) {
      printf("[RAW]  [%s] frame %3lu  first16:", frame->stream_name.c_str(), n);
      for (int i = 0; i < 16; i++)
        printf(" %02X", frame->data[i]);
      printf("\n");
    }

    // ── dump to disk after warm-up ────────────────────────────────────────
    if (!st.warmup_done.load(std::memory_order_relaxed))
      return;
    if (st.save_count.load(std::memory_order_relaxed) >= SAVE_FRAMES)
      return;
    if (!frame->data)
      return;

    uint32_t idx = st.save_count.fetch_add(1);
    std::string dir = save_base + frame->stream_name + "/";

    // raw binary dump
    std::string raw_path = dir + "frame_" + std::to_string(idx + 1) + ".raw";
    FILE * rf = fopen(raw_path.c_str(), "wb");
    if (rf) {
      size_t raw_size;
      if (frame->format == "nv12" || frame->format == "nv21") {
        // NV12/NV21: Y plane (stride × height) + UV plane (stride × height/2)
        raw_size = static_cast<size_t>(frame->stride) * frame->height * 3 / 2;
      } else {
        // mipi16: 2 bytes/pixel (stride already in bytes)
        // mipi8:  1 byte/pixel
        raw_size = static_cast<size_t>(frame->stride) * frame->height;
      }
      fwrite(frame->data, 1, raw_size, rf);
      fclose(rf);
      printf("[DUMP] %s\n", raw_path.c_str());
    }

    // JPEG: NV12 → 彩色 RGB；mipi16/mipi8 → 灰度
    std::string jpg_path = dir + "frame_" + std::to_string(idx + 1) + ".jpg";
    if (frame->format == "nv12" || frame->format == "nv21") {
      std::vector<uint8_t> rgb(frame->width * frame->height * 3);
      nv12_to_rgb(frame->data, rgb.data(), frame->width, frame->height, frame->stride);
      if (save_jpeg(jpg_path.c_str(), rgb.data(), static_cast<int>(frame->width),
              static_cast<int>(frame->height), 90)) {
        printf("[JPEG] %s  (color NV12)\n", jpg_path.c_str());
      }
    } else {
      std::vector<uint8_t> grey(frame->width * frame->height);
      if (frame->format == "mipi16") {
        mipi16_to_grey(frame->data, grey.data(), frame->width, frame->height, frame->stride);
      } else {
        mipi8_to_grey(frame->data, grey.data(), frame->width, frame->height, frame->stride);
      }
      if (save_grey_jpeg(jpg_path.c_str(), grey.data(), static_cast<int>(frame->width),
              static_cast<int>(frame->height), 90)) {
        printf("[JPEG] %s  (grey %s)\n", jpg_path.c_str(), frame->format.c_str());
      }
    }
  };

  if (!manager.register_callback(index, image_cb)) {
    std::cerr << "[ERROR] register_callback failed" << std::endl;
    return -1;
  }

  // ── 4. Start camera ───────────────────────────────────────────────────
  if (!manager.start_camera(index)) {
    std::cerr << "[ERROR] start_camera failed" << std::endl;
    return -1;
  }

  std::cout << "[INFO] Camera " << camera_id << " started (" << img_width << "x" << img_height
            << " @" << IMG_FPS << "fps" << ", 3 streams: stream0/stream1=mipi16, stream2=mipi8)."
            << "  Running for up to " << RUN_SECONDS << "s – Ctrl-C to stop early." << std::endl;

  // ── 5. Run until deadline or signal ──────────────────────────────────
  auto deadline = Clock::now() + std::chrono::seconds(RUN_SECONDS);
  while (g_running && Clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // ── 6. Stop cleanly ───────────────────────────────────────────────────
  manager.stop_camera(index);

  for (int i = 0; i < 3; ++i) {
    std::cout << "[INFO] [" << STREAMS[i].name
              << "] total frames received: " << states[i].frame_count.load()
              << "  saved: " << states[i].save_count.load() << std::endl;
  }
  return 0;
}
