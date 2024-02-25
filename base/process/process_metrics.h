// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains routines for gathering resource statistics for processes
// running on the system.

#ifndef BASE_PROCESS_PROCESS_METRICS_H_
#define BASE_PROCESS_PROCESS_METRICS_H_

namespace base {
  // /proc/self/exe refers to the current executable.
  BASE_EXPORT extern const char kProcSelfExe[];
}

#endif  // BASE_PROCESS_PROCESS_METRICS_H_
