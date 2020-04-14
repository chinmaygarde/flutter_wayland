#include "keyboard.h"

#include <wayland-client-protocol-extra.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <xkbcommon/xkbcommon.h>
#include <flutter_embedder.h>
#include <sys/mman.h>

#include <sys/wait.h>
#include <unistd.h>

#include "macros.h"

namespace flutter {

Keyboard::Keyboard(FlutterEngine engine, wayland::keyboard_t& keyb) : engine_(engine), keyb_(keyb)
{
    static wayland::keyboard_keymap_format keyb_format =
        wayland::keyboard_keymap_format::no_keymap;
    static struct xkb_context* xkb_context =
        xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    static struct xkb_keymap* keymap = NULL;
    static struct xkb_state* xkb_state = NULL;

    keyb_.on_keymap() = [&](wayland::keyboard_keymap_format format, int fd,
                               uint32_t size) {
      keyb_format = format;
      if (format == wayland::keyboard_keymap_format::xkb_v1) {
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

    keyb_.on_key() = [&](uint32_t, uint32_t, uint32_t key,
                            wayland::keyboard_key_state state) {
      if (keyb_format == wayland::keyboard_keymap_format::xkb_v1) {
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
            case wayland::keyboard_key_state::pressed:
              document.AddMember(kTypeKey, kKeyDown, allocator);
              break;
            case wayland::keyboard_key_state::released:
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
            FLWAY_LOG << "FlutterEngineSendPlatformMessage Result: " << result
                      << std::endl;
        } else {
          char name[64];
          xkb_keysym_get_name(keysym, name, 64);
          FLWAY_LOG << "the key " << name << " was "
                    << ((state == wayland::keyboard_key_state::pressed) ? "pressed"
                                                               : "released")
                    << std::endl;
        }
      }
    };

    keyb_.on_modifiers() = [&](uint32_t serial, uint32_t mods_depressed,
                                  uint32_t mods_latched, uint32_t mods_locked,
                                  uint32_t group) {
      xkb_state_update_mask(xkb_state, mods_depressed, mods_latched,
                            mods_locked, 0, 0, group);
    };
}

}