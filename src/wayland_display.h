// Copyright 2020 Joel Winarske. All rights reserved.
// Copyright 2018 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <array>
#include <iostream>
#include <stdexcept>
#include <wayland-client-protocol-extra.hpp>
#include <wayland-client.hpp>
#include <wayland-egl.hpp>

#include <GLES3/gl3.h>

#include <linux/input.h>

#include <flutter_embedder.h>

#include <functional>
#include <vector>

#include "macros.h"

using namespace wayland;

namespace flutter {

class WaylandDisplay {
 public:
  WaylandDisplay(size_t width,
                 size_t height,
                 std::string bundle_path,
                 const std::vector<std::string>& args);

  virtual ~WaylandDisplay() noexcept(false);

  bool IsValid() const;

  void InitializeApplication(std::string bundle_path,
                             const std::vector<std::string>& args);

  bool SetWindowSize(size_t width, size_t height);

  bool Run();

 private:
  // global objects
  display_t display;
  registry_t registry;
  compositor_t compositor;
  shell_t shell;
  xdg_wm_base_t xdg_wm_base;
  seat_t seat;
  shm_t shm;

  // local objects
  surface_t surface;
  shell_surface_t shell_surface;
  xdg_surface_t xdg_surface;
  xdg_toplevel_t xdg_toplevel;
  pointer_t pointer;
  keyboard_t keyboard;
  touch_t touch;

  // EGL
  egl_window_t egl_window;
  EGLDisplay egldisplay;
  EGLSurface eglsurface;
  EGLContext eglcontext;
  EGLContext eglresourcecontext;

  bool running;
  bool has_pointer;
  bool has_keyboard;
  bool has_touch;

  bool valid_ = false;
  const int screen_width_;
  const int screen_height_;
  int32_t cur_x;
  int32_t cur_y;

  FlutterEngine engine_ = nullptr;
  int last_button_ = 0;

  void init_egl();

  void ProcessEvents();

  bool GLMakeCurrent();
  bool GLClearCurrent();
  bool GLPresent();
  uint32_t GLFboCallback();
  bool GLMakeResourceCurrent();

  void PlatformMessageCallback(const FlutterPlatformMessage* message);

  FLWAY_DISALLOW_COPY_AND_ASSIGN(WaylandDisplay);
};

}  // namespace flutter
