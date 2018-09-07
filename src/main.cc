// Copyright 2018 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <string>
#include <vector>

#include "flutter_application.h"
#include "wayland_display.h"

namespace flutter {

bool Main(std::vector<std::string> args) {
  const size_t kWidth = 800;
  const size_t kHeight = 600;

  WaylandDisplay display(kWidth, kHeight);

  if (!display.IsValid()) {
    FLWAY_ERROR << "Wayland display was not valid." << std::endl;
    return false;
  }

  FlutterApplication application(args, display);
  if (!application.IsValid()) {
    FLWAY_ERROR << "Flutter application was not valid." << std::endl;
    return false;
  }

  if (!application.SetWindowSize(kWidth, kHeight)) {
    FLWAY_ERROR << "Could not update Flutter application size." << std::endl;
    return false;
  }

  display.Run();

  return true;
}

}  // namespace flutter

int main(int argc, char* argv[]) {
  std::vector<std::string> args;
  for (int i = 0; i < argc; ++i) {
    args.push_back(argv[i]);
  }
  return flutter::Main(std::move(args)) ? EXIT_SUCCESS : EXIT_FAILURE;
}
