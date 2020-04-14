#pragma once

#include <wayland-client-protocol-extra.hpp>
#include <flutter_embedder.h>


namespace flutter {

class Keyboard
{
  public:
    Keyboard(FlutterEngine engine, wayland::keyboard_t& keyb);

  private:
    FlutterEngine engine_;
    wayland::keyboard_t keyb_;

};

}