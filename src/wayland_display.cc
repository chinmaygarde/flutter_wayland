// Copyright 2020 Joel Winarske. All rights reserved.
// Copyright 2018 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wayland_display.h"

#include <linux/input-event-codes.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>

#include <chrono>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "utils.h"

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
#if 0  
  config.open_gl.gl_external_texture_frame_callback =
      [](void* context, int64_t texture_id, size_t width, size_t height,
         FlutterOpenGLTexture* texture) -> bool {
    return reinterpret_cast<WaylandDisplay*>(context)
        ->GLExternalTextureFrameCallback(texture_id, width, height, texture);
  };
#endif

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

  // Configure task runner interop
  FlutterTaskRunnerDescription platform_task_runner = {};
  platform_task_runner.struct_size = sizeof(FlutterTaskRunnerDescription);
  platform_task_runner.user_data = this;
  platform_task_runner.runs_task_on_current_thread_callback =
      [](void* context) -> bool { return true; };
  platform_task_runner.post_task_callback =
      [](FlutterTask task, uint64_t target_time, void* context) -> void {
    reinterpret_cast<WaylandDisplay*>(context)->PostTaskCallback(task,
                                                                 target_time);
  };

  FlutterCustomTaskRunners custom_task_runners = {};
  custom_task_runners.struct_size = sizeof(FlutterCustomTaskRunners);
  custom_task_runners.platform_task_runner = &platform_task_runner;
  args.custom_task_runners = &custom_task_runners;

  static constexpr char kAccessibilityChannel[] = "flutter/accessibility";
  static constexpr char kFlutterPlatformChannel[] = "flutter/platform";
  static constexpr char kTextInputChannel[] = "flutter/textinput";
  static constexpr char kKeyEventChannel[] = "flutter/keyevent";
  static constexpr char kFlutterPlatformViewsChannel[] = "flutter/platform_views";

  static constexpr char kPluginFlutterIoConnectivity[] = "plugins.flutter.io/connectivity";
  static constexpr char kPluginFlutterIoUrlLauncher[] = "plugins.flutter.io/url_launcher";

  platform_message_handlers_[kAccessibilityChannel] = std::bind(&WaylandDisplay::OnAccessibilityChannelPlatformMessage, this, std::placeholders::_1);
  platform_message_handlers_[kFlutterPlatformChannel] = std::bind(&WaylandDisplay::OnFlutterPlatformChannelPlatformMessage, this, std::placeholders::_1);
  platform_message_handlers_[kTextInputChannel] = std::bind(&WaylandDisplay::OnFlutterTextInputChannelPlatformMessage, this, std::placeholders::_1);
  platform_message_handlers_[kFlutterPlatformViewsChannel] = std::bind(&WaylandDisplay::OnFlutterPlatformViewsChannelPlatformMessage, this, std::placeholders::_1);

  platform_message_handlers_[kPluginFlutterIoConnectivity] = std::bind(&WaylandDisplay::OnFlutterPluginConnectivity, this, std::placeholders::_1);
  platform_message_handlers_[kPluginFlutterIoUrlLauncher] = std::bind(&WaylandDisplay::OnFlutterPluginIoUrlLauncher, this, std::placeholders::_1);

  FlutterEngine engine = nullptr;
  auto result = FlutterEngineInitialize(FLUTTER_ENGINE_VERSION, &config, &args,
                                        this, &engine_);
  if (result != kSuccess) {
    FLWAY_ERROR << "Could not Initialize the Flutter engine" << std::endl;
    return;
  }

  result = FlutterEngineRunInitialized(engine_);
  if (result != kSuccess) {
    FLWAY_ERROR << "Could not run the initialized Flutter engine" << std::endl;
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
          static constexpr char kLinuxKeyMap[] = "linux";
          static constexpr char kScanCodeKey[] = "scanCode";
          static constexpr char kModifiersKey[] = "modifiers";
          static constexpr char kTypeKey[] = "type";
          static constexpr char kToolkitKey[] = "toolkit";
          static constexpr char kGLFWKey[] = "glfw";
          static constexpr char kUnicodeScalarValues[] = "unicodeScalarValues";
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

    if (!TaskRunner.empty()) {
      uint64_t current = FlutterEngineGetCurrentTime();
      if (current >= TaskRunner.top().first) {
        auto item = TaskRunner.top();
        TaskRunner.pop();
        auto result = FlutterEngineRunTask(engine_, &item.second);
      }
    }
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

bool WaylandDisplay::GLExternalTextureFrameCallback(
    int64_t texture_id,
    size_t width,
    size_t height,
    FlutterOpenGLTexture* texture) {
  return true;
}

void WaylandDisplay::PostTaskCallback(FlutterTask task, uint64_t target_time) {
  TaskRunner.push(std::make_pair(target_time, task));
}

void WaylandDisplay::OnAccessibilityChannelPlatformMessage(const FlutterPlatformMessage* message)
{
  std::string msg;
  msg.assign(reinterpret_cast<const char *>(message->message), message->message_size);
  FLWAY_LOG << "AccessibilityChannel: " << msg << std::endl;
}

void WaylandDisplay::OnFlutterPlatformChannelPlatformMessage(const FlutterPlatformMessage* message)
{
  rapidjson::Document document;
  document.Parse(reinterpret_cast<const char*>(message->message), message->message_size);
  if (document.HasParseError() || !document.IsObject()) {
    return;
  }

  auto root = document.GetObject();
  auto method = root.FindMember("method");
  if (method == root.MemberEnd() || !method->value.IsString()) {
    return;
  }

  std::string msg;
  msg.assign(reinterpret_cast<const char *>(message->message), message->message_size);
  FLWAY_LOG << "PlatformChannel: " << method->value.GetString() << std::endl;

#if 0
  static constexpr char kSetApplicationSwitcherDescription[] = "SystemChrome.setApplicationSwitcherDescription";
  static constexpr char kSetSystemUiOverlayStyle[] = "SystemChrome.setSystemUIOverlayStyle";
  static constexpr char kSystemNavigatorPopMethod[] = "SystemNavigator.pop";
  static constexpr char kGetClipboardDataMethod[] = "Clipboard.getData";
  static constexpr char kSetClipboardDataMethod[] = "Clipboard.setData";
  static constexpr char kSystemSoundPlay[] = "SystemSound.play";
  static constexpr char kTextPlainFormat[] = "text/plain";
  static constexpr char kTextKey[] = "text";
#endif

  FlutterEngineSendPlatformMessageResponse(engine_, message->response_handle, nullptr, 0);
}

void WaylandDisplay::OnFlutterTextInputChannelPlatformMessage(const FlutterPlatformMessage* message)
{
  rapidjson::Document document;
  document.Parse(reinterpret_cast<const char*>(message->message), message->message_size);
  if (document.HasParseError() || !document.IsObject()) {
    return;
  }
  auto root = document.GetObject();
  auto method = root.FindMember("method");
  if (method == root.MemberEnd() || !method->value.IsString()) {
    return;
  }

  std::string msg;
  msg.assign(reinterpret_cast<const char *>(message->message), message->message_size);
  FLWAY_LOG << "TextInput: " << method->value.GetString() << std::endl;

#if 0
  if (method->value == "TextInput.show") {
    if (ime_) {
      text_sync_service_->ShowKeyboard();
    }
  } else if (method->value == "TextInput.hide") {
    if (ime_) {
      text_sync_service_->HideKeyboard();
    }
  } else 

  if (method->value == "TextInput.setClient") {
    current_text_input_client_ = 0;
    DeactivateIme();
    auto args = root.FindMember("args");
    if (args == root.MemberEnd() || !args->value.IsArray() ||
        args->value.Size() != 2)
      return;
    const auto& configuration = args->value[1];
    if (!configuration.IsObject()) {
      return;
    }
    // TODO(abarth): Read the keyboard type from the configuration.
    current_text_input_client_ = args->value[0].GetInt();

    auto initial_text_input_state = fuchsia::ui::input::TextInputState{};
    initial_text_input_state.text = "";
    last_text_state_ = std::make_unique<fuchsia::ui::input::TextInputState>(
        initial_text_input_state);
    ActivateIme();
  } else if (method->value == "TextInput.setEditingState") {
    if (ime_) {
      auto args_it = root.FindMember("args");
      if (args_it == root.MemberEnd() || !args_it->value.IsObject()) {
        return;
      }
      const auto& args = args_it->value;
      fuchsia::ui::input::TextInputState state;
      state.text = "";
      // TODO(abarth): Deserialize state.
      auto text = args.FindMember("text");
      if (text != args.MemberEnd() && text->value.IsString())
        state.text = text->value.GetString();
      auto selection_base = args.FindMember("selectionBase");
      if (selection_base != args.MemberEnd() && selection_base->value.IsInt())
        state.selection.base = selection_base->value.GetInt();
      auto selection_extent = args.FindMember("selectionExtent");
      if (selection_extent != args.MemberEnd() &&
          selection_extent->value.IsInt())
        state.selection.extent = selection_extent->value.GetInt();
      auto selection_affinity = args.FindMember("selectionAffinity");
      if (selection_affinity != args.MemberEnd() &&
          selection_affinity->value.IsString() &&
          selection_affinity->value == "TextAffinity.upstream")
        state.selection.affinity = fuchsia::ui::input::TextAffinity::UPSTREAM;
      else
        state.selection.affinity = fuchsia::ui::input::TextAffinity::DOWNSTREAM;
      // We ignore selectionIsDirectional because that concept doesn't exist on
      // Fuchsia.
      auto composing_base = args.FindMember("composingBase");
      if (composing_base != args.MemberEnd() && composing_base->value.IsInt())
        state.composing.start = composing_base->value.GetInt();
      auto composing_extent = args.FindMember("composingExtent");
      if (composing_extent != args.MemberEnd() &&
          composing_extent->value.IsInt())
        state.composing.end = composing_extent->value.GetInt();
      ime_->SetState(std::move(state));
    }
  } else if (method->value == "TextInput.clearClient") {
    current_text_input_client_ = 0;
    last_text_state_ = nullptr;
    DeactivateIme();
  } else {
    FLWAY_ERROR << "Unknown " << message->channel << " method "
                    << method->value.GetString();
  }
  #endif
}

void WaylandDisplay::OnFlutterPlatformViewsChannelPlatformMessage(const FlutterPlatformMessage* message)
{
  rapidjson::Document document;
  document.Parse(reinterpret_cast<const char*>(message->message), message->message_size);
  if (document.HasParseError() || !document.IsObject()) {
    FLWAY_ERROR << "Could not parse document";
    return;
  }
  auto root = document.GetObject();
  auto method = root.FindMember("method");
  if (method == root.MemberEnd() || !method->value.IsString()) {
    return;
  }

  std::string msg;
  msg.assign(reinterpret_cast<const char *>(message->message), message->message_size);
  FLWAY_LOG << "PlatformViews: " << method->value.GetString() << std::endl;

  if (method->value == "View.enableWireframe") {
    auto args_it = root.FindMember("args");
    if (args_it == root.MemberEnd() || !args_it->value.IsObject()) {
      FLWAY_ERROR << "No arguments found.";
      return;
    }
    const auto& args = args_it->value;

    auto enable = args.FindMember("enable");
    if (!enable->value.IsBool()) {
      FLWAY_ERROR << "Argument 'enable' is not a bool";
      return;
    }

    FLWAY_LOG << "wireframe_enabled_callback goes here" << std::endl;
  } else {
    FLWAY_ERROR << "Unknown " << message->channel << " method "
                    << method->value.GetString();
  }
}

void WaylandDisplay::OnFlutterPluginIoUrlLauncher(const FlutterPlatformMessage* message)
{
  std::unique_ptr<std::vector<std::uint8_t>> result;
  auto codec = &flutter::StandardMethodCodec::GetInstance();
  auto method_call = codec->DecodeMethodCall(message->message, message->message_size);

  if (method_call->method_name().compare("launch") == 0) {
    std::string url;
    if (method_call->arguments() && method_call->arguments()->IsMap()) {
      const EncodableMap &arguments = method_call->arguments()->MapValue();
      auto url_it = arguments.find(EncodableValue("url"));
      if (url_it != arguments.end()) {
        url = url_it->second.StringValue();
      }
    }
    if (url.empty()) {
      auto result = codec->EncodeErrorEnvelope("argument_error", "No URL provided");
      goto done;
    }

    pid_t pid = fork();
    if (pid == 0) {
      execl("/usr/bin/xdg-open", "xdg-open", url.c_str(), nullptr);
      exit(1);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (status != 0) {
      std::ostringstream error_message;
      error_message << "Failed to open " << url << ": error " << status;
      result = codec->EncodeErrorEnvelope("open_error", error_message.str());
      goto done;
    }
    auto val = EncodableValue(true);
    result = codec->EncodeSuccessEnvelope(&val);

  } else if (method_call->method_name().compare("canLaunch") == 0) {
    std::string url;
    if (method_call->arguments() && method_call->arguments()->IsMap()) {
      const EncodableMap &arguments = method_call->arguments()->MapValue();
      auto url_it = arguments.find(EncodableValue("url"));
      if (url_it != arguments.end()) {
        url = url_it->second.StringValue();
      }
    }
    if (url.empty()) {
      result = codec->EncodeErrorEnvelope("argument_error", "No URL provided");
      goto done;
    }
    flutter::EncodableValue response(
      (url.rfind("https:", 0) == 0) || (url.rfind("http:", 0) == 0) ||
      (url.rfind("ftp:", 0) == 0) || (url.rfind("file:", 0) == 0));
    result = codec->EncodeSuccessEnvelope(&response);
  }
done:
  FlutterEngineSendPlatformMessageResponse(engine_, message->response_handle, result->data(), result->size());
}

void WaylandDisplay::OnFlutterPluginConnectivity(const FlutterPlatformMessage* message)
{
  std::string msg;
  msg.assign(reinterpret_cast<const char *>(message->message), message->message_size);
  FLWAY_LOG << "Connectivity: " << msg << std::endl;

}

void WaylandDisplay::PlatformMessageCallback(
    const FlutterPlatformMessage* message) {

  // Find the handler for the channel; if there isn't one, report the failure.
  if (platform_message_handlers_.find(message->channel) == platform_message_handlers_.end()) {
    FlutterEngineSendPlatformMessageResponse(engine_, message->response_handle,
                                        nullptr, 0);
    return;
  }

  auto& message_handler = platform_message_handlers_[message->channel];

  message_handler(message);
}

}  // namespace flutter
