// Copyright 2018 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "flutter_application.h"
#include "wayland_display.h"

int main(int argc, char* argv[]) {
  auto application = std::make_unique<flutter::FlutterApplication>();

  if (!application->IsValid()) {
    FLWAY_ERROR << "Could not run the Flutter application." << std::endl;
    return EXIT_FAILURE;
  }

  flutter::WaylandDisplay display(std::move(application),  //
                                  "Flutter Gallery",       //
                                  800, 600);

  if (!display.IsValid()) {
    FLWAY_ERROR << "Could not create the wayland display." << std::endl;
    return EXIT_FAILURE;
  }

  display.Run();

  return EXIT_SUCCESS;
}
