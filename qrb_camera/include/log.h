// Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>

#ifndef LOG_TAG
#define LOG_TAG "log: "
#endif

#define PRI_INFO " I"
#define PRI_WARN " W"
#define PRI_ERROR " E"
#define PRI_DEBUG " D"
#define PRI_VERB " V"

#define LOG_ERROR LOG_ERR

#define ANDROID_LOG_UNKNOWN LOG_NOTICE
#define ANDROID_LOG_DEFAULT LOG_NOTICE
#define ANDROID_LOG_VERBOSE LOG_DEBUG
#define ANDROID_LOG_DEBUG LOG_DEBUG
#define ANDROID_LOG_INFO LOG_INFO
#define ANDROID_LOG_WARN LOG_WARNING
#define ANDROID_LOG_ERROR LOG_ERROR
#define ANDROID_LOG_FATAL LOG_ERROR
#define ANDROID_LOG_SILENT LOG_NOTICE

#define LOG_MSG_LENGTH 1024

#define __android_log_write(prio, tag, text) syslog (prio, "%s: %s", tag, text)
#define __android_log_print(prio, tag, fmt, ...) \
  do { \
    char buffer[LOG_MSG_LENGTH]; \
    snprintf(buffer, sizeof(buffer), fmt, ##__VA_ARGS__); \
    syslog(prio, "%s: %s", tag, buffer); \
  } while (0)

#define ALOGV(fmt, arg...) syslog (LOG_NOTICE, LOG_TAG fmt, ##arg)
#define ALOGD(fmt, arg...) syslog (LOG_DEBUG, LOG_TAG fmt, ##arg)
#define ALOGI(fmt, arg...) syslog (LOG_INFO, LOG_TAG fmt, ##arg)
#define ALOGW(fmt, arg...) syslog (LOG_WARNING, LOG_TAG fmt, ##arg)
#define ALOGE(fmt, arg...) syslog (LOG_ERROR, LOG_TAG fmt, ##arg)
