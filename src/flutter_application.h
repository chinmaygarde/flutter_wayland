// Copyright 2018 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <flutter_embedder.h>

#include <functional>
#include <map>
#include <mutex>

#include "macros.h"

namespace flutter {

class FlutterApplication {
 public:
  FlutterApplication();

  ~FlutterApplication();

  bool IsValid() const;

  void ProcessEvents();

  bool SetWindowSize(size_t width, size_t height);

  bool SendPointerEvent(int button, int x, int y);

  using PresentCallback = std::function<
      void(const void* allocation, size_t row_bytes, size_t height)>;
  void SetOnPresentCallback(PresentCallback callback);

 private:
  bool valid_;
  FlutterEngine engine_ = nullptr;
  std::mutex mutex_;
  PresentCallback present_callback_;
  int last_button_ = 0;

  static bool PresentSurface(void* user_data,
                             const void* allocation,
                             size_t row_bytes,
                             size_t height);

  bool PresentSurface(const void* allocation, size_t row_bytes, size_t height);

  bool SendFlutterPointerEvent(FlutterPointerPhase phase, double x, double y);

  FLWAY_DISALLOW_COPY_AND_ASSIGN(FlutterApplication);
};

}  // namespace flutter
