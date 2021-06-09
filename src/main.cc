// Copyright 2020 Joel Winarske. All rights reserved.
// Copyright 2018 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <string>
#include <vector>

#include "utils.h"
#include "wayland_display.h"

namespace flutter {

static void PrintUsage() {
  std::cerr << "Flutter Wayland Embedder" << std::endl << std::endl;
  std::cerr << "========================" << std::endl;
  std::cerr << "Usage: `" << GetExecutableName()
            << " <asset_path> <flutter_flags>`" << std::endl
            << std::endl;
  std::cerr << R"~(
This utility runs an instance of a Flutter application and renders using
Wayland core protocols.

The Flutter tools can be obtained at https://flutter.io/

app_path:      This either points to asset bundle path, or
               an Ahead Of Time (AOT) shared library (.so).

asset_path:    The Flutter application code needs to be snapshotted using
               the Flutter tools and the assets packaged in the appropriate
               location. This can be done for any Flutter application by
               running `flutter build bundle` while in the directory of a
               valid Flutter project. This should package all the code and
               assets in the "build/flutter_assets" directory. Specify this
               directory as the first argument to this utility.

flutter_flags: Typically empty. These extra flags are passed directly to the
               Flutter engine. To see all supported flags, run
               `flutter_tester --help` using the test binary included in the
               Flutter tools.
)~" << std::endl;
}

static bool Main(std::vector<std::string> args) {
  if (args.size() == 0) {
    std::cerr << "   <Invalid Arguments>   " << std::endl;
    PrintUsage();
    return false;
  }

  const auto assets_path = args[0];

  if (!FlutterAssetsPathIsValid(assets_path)) {
    std::cerr << "   <Invalid Application>   " << std::endl;
    PrintUsage();
    return false;
  }

  const size_t kWidth = 1280;
  const size_t kHeight = 1024;

  for (const auto& arg : args) {
    FLWAY_LOG << "Arg: " << arg << std::endl;
  }

  WaylandDisplay display(kWidth, kHeight, args);

  if (!display.IsValid()) {
    FLWAY_ERROR << "Wayland display was not valid." << std::endl;
    return false;
  }

  display.InitializeApplication(assets_path, args);
  if (!display.IsValid()) {
    FLWAY_ERROR << "Flutter application was not valid." << std::endl;
    return false;
  }

  if (!display.SetWindowSize(kWidth, kHeight)) {
    FLWAY_ERROR << "Could not update Flutter application size." << std::endl;
    return false;
  }

  display.Run();

  return true;
}

}  // namespace flutter

int main(int argc, char* argv[]) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) {
    args.push_back(argv[i]);
  }
  return flutter::Main(std::move(args)) ? EXIT_SUCCESS : EXIT_FAILURE;
}
