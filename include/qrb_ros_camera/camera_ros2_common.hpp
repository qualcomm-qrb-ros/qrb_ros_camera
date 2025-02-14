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
#ifndef __CAMERA_ROS2_COMMON_H__
#define __CAMERA_ROS2_COMMON_H__
#include <linux/in.h>
#include <linux/un.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <tinyxml2.h>

#include <functional>
// ROS2 headers
#include <sensor_msgs/image_encodings.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <std_msgs/msg/string.hpp>

#include "rclcpp/rclcpp.hpp"

#define MAX_CAMERA_NUM (5)
#define MAX_CAMERA_ROS2_PIPELINES MAX_CAMERA_NUM
#define CAMERAROS2_WORKDIR "/var/tmp/ros2"
#define SUCCESS 0
#define FAILED -1
#define CameraRos2Max(a, b) ((a) >= (b) ? (a) : (b))

typedef enum _CameraRos2Fmt
{
  CameraRos2Fmt_INVALID = 0,
  CameraRos2Fmt_YUV = 1,
  CameraRos2Fmt_JPEG = 2,
  CameraRos2Fmt_RGB = 3,
  CameraRos2Fmt_RAW = 4,
  CameraRos2Fmt_MONO = 5,
} CameraRos2Fmt;

typedef enum _CameraRos2LatencyType
{
  CameraRos2LatencyShutter = 0,
  CameraRos2LatencyDistribution = 1,
} CameraRos2LatencyType;

typedef struct _CameraRos2FrameInfo
{
  uint32_t pipeline_id;
  int32_t camera_id;
  uint32_t frame_id;
  uint64_t size;
  uint32_t width;
  uint32_t height;
  uint32_t stride;
  uint32_t slice;
  std::string format;
  int64_t timestamp;  // ns
  int64_t latency;    // ns
  uint8_t latency_type;
  int32_t fd;

} CameraRos2FrameInfo;
typedef struct _CameraRos2Frame
{
  CameraRos2FrameInfo info;
  uint8_t * data;
} CameraRos2Frame;

typedef std::function<void(CameraRos2Frame * frame)> CameraRos2MsgPublishFrameFunc;

static inline int32_t get_tid()
{
  return (int32_t)syscall(SYS_gettid);
}

static void camera_ros2_log(const char * p_format, ...)
{
  char log_text[512];
  va_list args;
  va_start(args, p_format);
  vsnprintf(log_text, sizeof(log_text), p_format, args);
  va_end(args);
  struct timespec spec;
  clock_gettime(CLOCK_REALTIME, &spec);
  struct tm * pCurrentTime = localtime((time_t *)&spec.tv_sec);
  if (pCurrentTime != nullptr) {
    printf("%02d-%02d %02d:%02d:%02d:%09ld %5d %5d %s\n", (pCurrentTime->tm_mon + 1),
        pCurrentTime->tm_mday, pCurrentTime->tm_hour, pCurrentTime->tm_min, pCurrentTime->tm_sec,
        spec.tv_nsec, getpid(), get_tid(), log_text);
  } else {
    printf("%5d %5d %s\n", getpid(), get_tid(), log_text);
  }
}

static inline CameraRos2Fmt get_format_type(std::string format)
{
  if (format == "nv12" || format == "YUY2") {
    return CameraRos2Fmt_YUV;
  } else if (format == "JPEG") {
    return CameraRos2Fmt_JPEG;
  } else if (format == "rgb8") {
    return CameraRos2Fmt_RGB;
  } else if (format == "mono8") {
    return CameraRos2Fmt_MONO;
  } else if (format.npos != format.find("RAW")) {
    return CameraRos2Fmt_RAW;
  }
  return CameraRos2Fmt_INVALID;
}

static inline const char * get_file_type(CameraRos2Fmt format)
{
  if (format == CameraRos2Fmt_YUV) {
    return "yuv";
  } else if (format == CameraRos2Fmt_RAW) {
    return "raw";
  } else if (format == CameraRos2Fmt_JPEG) {
    return "jpeg";
  } else if (format == CameraRos2Fmt_RGB) {
    return "rgb";
  } else if (format == CameraRos2Fmt_MONO) {
    return "mono8";
  }
  return "unknown";
}

static inline uint32_t align(uint32_t operand, uint32_t alignment)
{
  uint32_t remainder = (operand % alignment);
  return (0 == remainder) ? operand : operand - remainder + alignment;
}

static void get_alignment(std::string format,
    uint32_t width,
    uint32_t height,
    uint32_t * stride,
    uint32_t * slice)
{
  if (stride == nullptr || slice == nullptr) {
    return;
  }
  CameraRos2Fmt fmt = get_format_type(format);
  switch (fmt) {
    case CameraRos2Fmt_YUV: {
#ifdef KONA
      /// 512 Alignment hardcode for kona venius buffer
      *stride = align(width, 512);
      *slice = align(height, 512);
#else
      // for Kailua, width align 128, height align 32.
      *stride = align(width, 128);
      *slice = align(height, 32);
#endif
      break;
    }
    case CameraRos2Fmt_MONO: {
#ifdef KONA
      *stride = align(width, 512);
      *slice = align(height, 512);
#else
      // for Kailua, width align 128, height align 32.
      *stride = align(width, 128);
      *slice = align(height, 32);
#endif
      break;
    }
    case CameraRos2Fmt_RGB: {
      *stride = width * 3;
      *slice = height;
      break;
    }
    default: {
      *stride = width;
      *slice = height;
      break;
    }
  }
}
#define S2NS(s) (s * 1000000000LL)

#endif
