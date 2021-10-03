#pragma once

#include <flutter_embedder.h>

#include <wayland-client-protocol-extra.hpp>

namespace flutter {

class Keyboard {
 public:
  Keyboard(FlutterEngine engine, wayland::keyboard_t& keyb);

 private:
  FlutterEngine engine_;
  wayland::keyboard_t keyb_;
};

}  // namespace flutter