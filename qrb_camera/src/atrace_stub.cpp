/*
 *​​​​ Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
// libqcxclient.so was built for Android where these symbols come from libcutils.
// On Linux they are absent, so provide no-op stubs to satisfy the linker.
#include <cstdint>

extern "C" {
volatile int32_t atrace_enabled_tags = 0;
volatile int32_t atrace_is_ready     = 0;
int              atrace_marker_fd    = -1;
void atrace_update_tags(void) {}
void atrace_setup(void) {}
void atrace_begin_body(const char *) {}
}
