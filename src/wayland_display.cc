// Copyright 2020 Joel Winarske. All rights reserved.
// Copyright 2018 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wayland_display.h"
#include "utils.h"

#include <linux/input-event-codes.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>
#include <chrono>
#include <sstream>
#include <thread>
#include <vector>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace flutter {

static const char* kICUDataFileName = "icudtl.dat";

static std::string GetICUDataPath() {
  auto exe_dir = GetExecutableDirectory();
  if (exe_dir == "") {
    return "";
  }
  std::stringstream stream;
  stream << exe_dir << kICUDataFileName;

  auto icu_path = stream.str();

  if (!FileExistsAtPath(icu_path.c_str())) {
    FLWAY_ERROR << "Could not find " << icu_path << std::endl;
    return "";
  }

  return icu_path;
}

void WaylandDisplay::InitializeApplication(
    std::string bundle_path,
    const std::vector<std::string>& command_line_args) {
  if (!FlutterAssetBundleIsValid(bundle_path)) {
    FLWAY_ERROR << "Flutter asset bundle was not valid." << std::endl;
    return;
  }

  FlutterRendererConfig config = {};
  config.type = kOpenGL;
  config.open_gl.struct_size = sizeof(config.open_gl);
  config.open_gl.make_current = [](void* context) -> bool {
    return reinterpret_cast<WaylandDisplay*>(context)->GLMakeCurrent();
  };
  config.open_gl.clear_current = [](void* context) -> bool {
    return reinterpret_cast<WaylandDisplay*>(context)->GLClearCurrent();
  };
  config.open_gl.present = [](void* context) -> bool {
    return reinterpret_cast<WaylandDisplay*>(context)->GLPresent();
  };
  config.open_gl.fbo_callback = [](void* context) -> uint32_t {
    return reinterpret_cast<WaylandDisplay*>(context)->GLFboCallback();
  };
  config.open_gl.make_resource_current = [](void* context) -> bool {
    return reinterpret_cast<WaylandDisplay*>(context)->GLMakeResourceCurrent();
  };

  config.open_gl.gl_proc_resolver = [](void* context,
                                       const char* name) -> void* {
    auto address = eglGetProcAddress(name);
    if (address != nullptr) {
      return reinterpret_cast<void*>(address);
    }
    FLWAY_ERROR << "Tried unsuccessfully to resolve: " << name << std::endl;
    return nullptr;
  };

  auto icu_data_path = GetICUDataPath();

  if (icu_data_path == "") {
    FLWAY_ERROR << "Could not find ICU data. It should be placed next to the "
                   "executable but it wasn't there."
                << std::endl;
    return;
  }

  std::vector<const char*> command_line_args_c;

  for (const auto& arg : command_line_args) {
    command_line_args_c.push_back(arg.c_str());
  }

  FlutterProjectArgs args = {};
  args.struct_size = sizeof(FlutterProjectArgs);
  args.assets_path = bundle_path.c_str();
  args.icu_data_path = icu_data_path.c_str();
  args.command_line_argc = static_cast<int>(command_line_args_c.size());
  args.command_line_argv = command_line_args_c.data();
  args.platform_message_callback = [](const FlutterPlatformMessage* message,
                                      void* context) {
    reinterpret_cast<WaylandDisplay*>(context)->PlatformMessageCallback(
        message);
  };

  FlutterEngine engine = nullptr;
  auto result = FlutterEngineRun(FLUTTER_ENGINE_VERSION, &config, &args,
                                 this /* userdata */, &engine_);

  if (result != kSuccess) {
    FLWAY_ERROR << "Could not run the Flutter engine" << std::endl;
    return;
  }

  valid_ = true;
}

bool WaylandDisplay::IsValid() const {
  return valid_;
}

bool WaylandDisplay::SetWindowSize(size_t width, size_t height) {
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = width;
  event.height = height;
  event.pixel_ratio = 1.0;
  return FlutterEngineSendWindowMetricsEvent(engine_, &event) == kSuccess;
}

void WaylandDisplay::ProcessEvents() {
  __FlutterEngineFlushPendingTasksNow();
}

WaylandDisplay::WaylandDisplay(size_t width,
                               size_t height,
                               std::string bundle_path,
                               const std::vector<std::string>& args)
    : screen_width_(width), screen_height_(height) {
  if (screen_width_ == 0 || screen_height_ == 0) {
    FLWAY_ERROR << "Invalid screen dimensions." << std::endl;
    return;
  }

  // retrieve global objects
  registry = display.get_registry();
  registry.on_global() = [&](uint32_t name, std::string interface,
                             uint32_t version) {
    if (interface == compositor_t::interface_name)
      registry.bind(name, compositor, version);
    else if (interface == shell_t::interface_name)
      registry.bind(name, shell, version);
    else if (interface == xdg_wm_base_t::interface_name)
      registry.bind(name, xdg_wm_base, version);
    else if (interface == seat_t::interface_name)
      registry.bind(name, seat, version);
    else if (interface == shm_t::interface_name)
      registry.bind(name, shm, version);
  };
  display.roundtrip();

  seat.on_capabilities() = [&](seat_capability capability) {
    has_keyboard = capability & seat_capability::keyboard;
    has_pointer = capability & seat_capability::pointer;
    has_touch = capability & seat_capability::touch;
  };

  // create a surface
  surface = compositor.create_surface();

  // create a shell surface
  if (xdg_wm_base) {
    xdg_wm_base.on_ping() = [&](uint32_t serial) { xdg_wm_base.pong(serial); };
    xdg_surface = xdg_wm_base.get_xdg_surface(surface);
    xdg_surface.on_configure() = [&](uint32_t serial) {
      xdg_surface.ack_configure(serial);
    };
    xdg_toplevel = xdg_surface.get_toplevel();
    xdg_toplevel.set_title("Window");
    xdg_toplevel.on_close() = [&]() { running = false; };
  } else {
    shell_surface = shell.get_shell_surface(surface);
    shell_surface.on_ping() = [&](uint32_t serial) {
      shell_surface.pong(serial);
    };
    shell_surface.set_title("Flutter");
    shell_surface.set_toplevel();
  }
  surface.commit();

  display.roundtrip();

  if (has_touch) {
    std::cout << "Touch Present" << std::endl;
    touch = seat.get_touch();
    static FlutterPointerPhase TouchPhase = kUp;

    touch.on_down() = [&](uint32_t serial, uint32_t time, surface_t surface,
                          int32_t id, double x, double y) {
      FlutterPointerEvent event = {};
      event.struct_size = sizeof(event);
      event.phase = kDown;
      TouchPhase = event.phase;
      cur_x = x;
      cur_y = y;
      event.x = x;
      event.y = y;
      event.timestamp =
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::high_resolution_clock::now().time_since_epoch())
              .count();
      FlutterEngineSendPointerEvent(engine_, &event, 1);
    };

    touch.on_up() = [&](uint32_t serial, uint32_t time, int32_t id) {
      FlutterPointerEvent event = {};
      event.struct_size = sizeof(event);
      event.phase = kUp;
      TouchPhase = event.phase;
      event.x = cur_x;
      event.y = cur_y;
      event.timestamp =
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::high_resolution_clock::now().time_since_epoch())
              .count();
      FlutterEngineSendPointerEvent(engine_, &event, 1);
    };

    touch.on_motion() = [&](uint32_t time, int32_t id, double x, double y) {
      cur_x = x;
      cur_y = y;

      FlutterPointerEvent event = {};
      event.struct_size = sizeof(event);
      event.phase = (TouchPhase == kUp) ? kHover : kMove;
      event.x = cur_x;
      event.y = cur_y;
      event.timestamp =
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::high_resolution_clock::now().time_since_epoch())
              .count();
      FlutterEngineSendPointerEvent(engine_, &event, 1);
    };
  }

  if (has_pointer) {
    std::cout << "Pointer Present" << std::endl;
    pointer = seat.get_pointer();
    static FlutterPointerPhase PointerPhase = kUp;

    pointer.on_enter() = [&](uint32_t serial, surface_t surface,
                             int32_t surface_x, int32_t surface_y) {
      FlutterPointerEvent event = {};
      event.struct_size = sizeof(event);
      event.phase = kAdd;
      event.x = surface_x;
      event.y = surface_y;
      event.timestamp =
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::high_resolution_clock::now().time_since_epoch())
              .count();
      FlutterEngineSendPointerEvent(engine_, &event, 1);
    };

    pointer.on_leave() = [&](uint32_t serial, surface_t surface) {
      FlutterPointerEvent event = {};
      event.struct_size = sizeof(event);
      event.phase = kRemove;
      event.timestamp =
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::high_resolution_clock::now().time_since_epoch())
              .count();
      FlutterEngineSendPointerEvent(engine_, &event, 1);
    };

    static bool button = false;
    pointer.on_motion() = [&](uint32_t time, double surface_x,
                              double surface_y) {
      cur_x = surface_x;
      cur_y = surface_y;

      FlutterPointerEvent event = {};
      event.struct_size = sizeof(event);
      event.phase = (PointerPhase == kUp) ? kHover : kMove;
      event.x = surface_x;
      event.y = surface_y;
      event.timestamp =
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::high_resolution_clock::now().time_since_epoch())
              .count();
      FlutterEngineSendPointerEvent(engine_, &event, 1);
    };

    pointer.on_button() = [&](uint32_t serial, uint32_t time, uint32_t button,
                              pointer_button_state state) {
      FlutterPointerEvent event = {};
      event.struct_size = sizeof(event);
      event.phase = (state == pointer_button_state::pressed) ? kDown : kUp;
      PointerPhase = event.phase;
      switch (button) {
        case BTN_LEFT:
          event.buttons = kFlutterPointerButtonMousePrimary;
          break;
        case BTN_RIGHT:
          event.buttons = kFlutterPointerButtonMouseSecondary;
          break;
        case BTN_MIDDLE:
          event.buttons = kFlutterPointerButtonMouseMiddle;
          break;
      }
      event.x = cur_x;
      event.y = cur_y;
      event.timestamp =
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::high_resolution_clock::now().time_since_epoch())
              .count();
      FlutterEngineSendPointerEvent(engine_, &event, 1);
    };

    pointer.on_axis() = [&](uint32_t time, pointer_axis axis, double value) {
      std::cout << "OnAxis time: " << time
                << " axis: " << static_cast<uint32_t>(axis)
                << " value: " << value << std::endl;
    };
  }

  if (has_keyboard) {
    std::cout << "Keyboard Present" << std::endl;
    keyboard = seat.get_keyboard();

    static keyboard_keymap_format keyb_format =
        keyboard_keymap_format::no_keymap;
    static struct xkb_context* xkb_context =
        xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    static struct xkb_keymap* keymap = NULL;
    static struct xkb_state* xkb_state = NULL;

    keyboard.on_keymap() = [&](keyboard_keymap_format format, int fd,
                               uint32_t size) {
      keyb_format = format;
      if (format == keyboard_keymap_format::xkb_v1) {
        char* keymap_string = reinterpret_cast<char*>(
            mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0));
        xkb_keymap_unref(keymap);
        keymap = xkb_keymap_new_from_string(xkb_context, keymap_string,
                                            XKB_KEYMAP_FORMAT_TEXT_V1,
                                            XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(keymap_string, size);
        close(fd);
        xkb_state_unref(xkb_state);
        xkb_state = xkb_state_new(keymap);
      }
    };

    keyboard.on_key() = [&](uint32_t, uint32_t, uint32_t key,
                            keyboard_key_state state) {
      if (keyb_format == keyboard_keymap_format::xkb_v1) {
        xkb_keysym_t keysym = xkb_state_key_get_one_sym(xkb_state, key + 8);
        uint32_t utf32 = xkb_keysym_to_utf32(keysym);
        if (utf32) {
          static constexpr char kChannelName[] = "flutter/keyevent";

          static constexpr char kKeyCodeKey[] = "keyCode";
          static constexpr char kKeyMapKey[] = "keymap";
          static constexpr char kScanCodeKey[] = "scanCode";
          static constexpr char kModifiersKey[] = "modifiers";
          static constexpr char kTypeKey[] = "type";
          static constexpr char kToolkitKey[] = "toolkit";
          static constexpr char kUnicodeScalarValues[] = "unicodeScalarValues";
          static constexpr char kLinuxKeyMap[] = "linux";
          static constexpr char kGLFWKey[] = "glfw";
          static constexpr char kKeyUp[] = "keyup";
          static constexpr char kKeyDown[] = "keydown";

          rapidjson::Document document;
          auto& allocator = document.GetAllocator();
          document.SetObject();
          document.AddMember(kKeyCodeKey, key, allocator);
          document.AddMember(kKeyMapKey, kLinuxKeyMap, allocator);
          document.AddMember(kScanCodeKey, key + 8, allocator);
          document.AddMember(kModifiersKey, 0, allocator);
          document.AddMember(kToolkitKey, kGLFWKey, allocator);
          document.AddMember(kUnicodeScalarValues, utf32, allocator);

          switch (state) {
            case keyboard_key_state::pressed:
              document.AddMember(kTypeKey, kKeyDown, allocator);
              break;
            case keyboard_key_state::released:
              document.AddMember(kTypeKey, kKeyUp, allocator);
              break;
          }

          rapidjson::StringBuffer buffer;
          rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
          document.Accept(writer);
          std::cout << buffer.GetString() << std::endl;

          FlutterPlatformMessage message = {};
          message.struct_size = sizeof(FlutterPlatformMessage);
          message.channel = kChannelName;
          message.message =
              reinterpret_cast<const uint8_t*>(buffer.GetString());
          message.message_size = buffer.GetSize();
          message.response_handle = nullptr;

          auto result = FlutterEngineSendPlatformMessage(engine_, &message);
          if (result != kSuccess)
            std::cout << "FlutterEngineSendPlatformMessage Result: " << result
                      << std::endl;
        } else {
          char name[64];
          xkb_keysym_get_name(keysym, name, 64);
          std::cout << "the key " << name << " was "
                    << ((state == keyboard_key_state::pressed) ? "pressed"
                                                               : "released")
                    << std::endl;
        }
      }
    };

    keyboard.on_modifiers() = [&](uint32_t serial, uint32_t mods_depressed,
                                  uint32_t mods_latched, uint32_t mods_locked,
                                  uint32_t group) {
      xkb_state_update_mask(xkb_state, mods_depressed, mods_latched,
                            mods_locked, 0, 0, group);
    };
  }

  // intitialize egl
  egl_window = egl_window_t(surface, screen_width_, screen_height_);
  init_egl();

  valid_ = true;
}

WaylandDisplay::~WaylandDisplay() noexcept(false) {
  if (engine_ != nullptr) {
    auto result = FlutterEngineShutdown(engine_);

    if (result != kSuccess) {
      FLWAY_ERROR << "Could not shutdown the Flutter engine." << std::endl;
    }
  }

  // finialize EGL
  if (eglDestroyContext(egldisplay, eglcontext) == EGL_FALSE)
    throw std::runtime_error("eglDestroyContext");
  if (eglDestroyContext(egldisplay, eglresourcecontext) == EGL_FALSE)
    throw std::runtime_error("eglDestroyContext Resource");
  if (eglTerminate(egldisplay) == EGL_FALSE)
    throw std::runtime_error("eglTerminate");
}

void WaylandDisplay::init_egl() {
  if (eglBindAPI(EGL_OPENGL_API) == EGL_FALSE)
    throw std::runtime_error("eglBindAPI");

  egldisplay = eglGetDisplay(display);
  if (egldisplay == EGL_NO_DISPLAY)
    throw std::runtime_error("No EGL Display..");

  EGLint major, minor;
  if (eglInitialize(egldisplay, &major, &minor) == EGL_FALSE)
    throw std::runtime_error("eglInitialize");
  if (!((major == 1 && minor >= 4) || major >= 2))
    throw std::runtime_error("EGL version too old");

  std::array<EGLint, 17> config_attribs = {{
      // clang-format off
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,
        EGL_DEPTH_SIZE,      0,
        EGL_STENCIL_SIZE,    0,
        EGL_NONE,            // termination sentinel
      // clang-format on
  }};

  EGLConfig config;
  EGLint num;
  if (eglChooseConfig(egldisplay, config_attribs.data(), &config, 1, &num) ==
          EGL_FALSE ||
      num == 0)
    throw std::runtime_error("eglChooseConfig");

  eglsurface = eglCreateWindowSurface(egldisplay, config, egl_window, nullptr);
  if (eglsurface == EGL_NO_SURFACE)
    throw std::runtime_error("eglCreateWindowSurface");

  std::array<EGLint, 3> context_attribs = {
      {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE}};

  eglcontext = eglCreateContext(egldisplay, config, EGL_NO_CONTEXT,
                                context_attribs.data());
  if (eglcontext == EGL_NO_CONTEXT)
    throw std::runtime_error("eglCreateContext");

  eglresourcecontext =
      eglCreateContext(egldisplay, config, eglcontext, context_attribs.data());
  if (eglresourcecontext == EGL_NO_CONTEXT)
    throw std::runtime_error("eglCreateContext");
}

bool WaylandDisplay::Run() {
  if (!valid_) {
    FLWAY_ERROR << "Could not run an invalid display." << std::endl;
    return false;
  }

  running = true;

  // event loop
  while (running && valid_) {
    display.dispatch();
  }

  return true;
}

bool WaylandDisplay::GLMakeCurrent() {
  if (!valid_) {
    FLWAY_ERROR << "Invalid display." << std::endl;
    return false;
  }

  return (eglMakeCurrent(egldisplay, eglsurface, eglsurface, eglcontext) ==
          EGL_TRUE);
}

bool WaylandDisplay::GLClearCurrent() {
  if (!valid_) {
    FLWAY_ERROR << "Invalid display." << std::endl;
    return false;
  }

  return (eglMakeCurrent(egldisplay, EGL_NO_SURFACE, EGL_NO_SURFACE,
                         EGL_NO_CONTEXT) == EGL_TRUE);
}

bool WaylandDisplay::GLPresent() {
  if (!valid_) {
    FLWAY_ERROR << "Invalid display." << std::endl;
    return false;
  }

  return (eglSwapBuffers(egldisplay, eglsurface) == EGL_TRUE);
}

uint32_t WaylandDisplay::GLFboCallback() {
  if (!valid_) {
    FLWAY_ERROR << "Invalid display." << std::endl;
    return 999;
  }

  return 0;  // FBO0
}

bool WaylandDisplay::GLMakeResourceCurrent() {
  if (!valid_) {
    FLWAY_ERROR << "Invalid display." << std::endl;
    return false;
  }

  return (eglMakeCurrent(egldisplay, EGL_NO_SURFACE, EGL_NO_SURFACE,
                         eglresourcecontext) == EGL_TRUE);
}

void WaylandDisplay::PlatformMessageCallback(
    const FlutterPlatformMessage* message) {
  FLWAY_LOG << "Channel: " << message->channel << std::endl;
  FLWAY_LOG << "Message Size: " << message->message_size << std::endl;
  FLWAY_LOG << "Message: " << message->message << std::endl;

  if (message->response_handle != nullptr) {
    auto res = FlutterEngineSendPlatformMessageResponse(
        engine_, message->response_handle, nullptr, 0);
  }
}

}  // namespace flutter
