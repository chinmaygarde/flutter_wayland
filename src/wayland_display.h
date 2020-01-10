// Copyright 2018 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <EGL/egl.h>
#include <wayland-client.h>
#include <wayland-egl.h>

#include <memory>
#include <string>

#include "flutter_application.h"
#include "macros.h"

namespace flutter {

class WaylandDisplay : public FlutterApplication::RenderDelegate {
 public:
  class InputDelegate {
   public:
    virtual void OnDisplayPointerEnter(uint32_t x, uint32_t y) = 0;

    virtual void OnDisplayPointerLeave() = 0;

    virtual void OnDisplayPointerMotion(uint32_t x, uint32_t y) = 0;

    virtual void OnDisplayPointerButton(uint32_t button, uint32_t status) = 0;

    virtual void OnDisplayPointerAxis(uint32_t axis, uint32_t value) = 0;
  };

  WaylandDisplay(size_t width, size_t height, InputDelegate& input_delegate);

  ~WaylandDisplay();

  bool IsValid() const;

  bool Run();

 private:
  static const wl_registry_listener kRegistryListener;
  static const wl_shell_surface_listener kShellSurfaceListener;
  static const wl_seat_listener kSeatListener;
  static const wl_pointer_listener kPointerListener;

  InputDelegate& input_delegate_;
  bool valid_ = false;
  const int screen_width_;
  const int screen_height_;
  wl_display* display_ = nullptr;
  wl_registry* registry_ = nullptr;
  wl_compositor* compositor_ = nullptr;
  wl_shell* shell_ = nullptr;
  wl_seat* seat_ = nullptr;
  wl_pointer* pointer_ = nullptr;
  wl_shell_surface* shell_surface_ = nullptr;
  wl_surface* surface_ = nullptr;
  wl_egl_window* window_ = nullptr;
  EGLDisplay egl_display_ = EGL_NO_DISPLAY;
  EGLSurface egl_surface_ = nullptr;
  EGLContext egl_context_ = EGL_NO_CONTEXT;

  bool SetupEGL();

  void AnnounceRegistryInterface(struct wl_registry* wl_registry,
                                 uint32_t name,
                                 const char* interface,
                                 uint32_t version);

  void UnannounceRegistryInterface(struct wl_registry* wl_registry,
                                   uint32_t name);

  bool StopRunning();

  // |flutter::FlutterApplication::RenderDelegate|
  bool OnApplicationContextMakeCurrent() override;

  // |flutter::FlutterApplication::RenderDelegate|
  bool OnApplicationContextClearCurrent() override;

  // |flutter::FlutterApplication::RenderDelegate|
  bool OnApplicationPresent() override;

  // |flutter::FlutterApplication::RenderDelegate|
  uint32_t OnApplicationGetOnscreenFBO() override;

  FLWAY_DISALLOW_COPY_AND_ASSIGN(WaylandDisplay);
};

}  // namespace flutter
