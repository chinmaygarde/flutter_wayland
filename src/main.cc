// Copyright 2018 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <string>
#include <vector>

#include "flutter_application.h"
#include "utils.h"
#include "wayland_display.h"

namespace flutter {

static void PrintUsage() {
  std::cerr << "Flutter Wayland Embedder" << std::endl << std::endl;
  std::cerr << "========================" << std::endl;
  std::cerr << "Usage: `" << GetExecutableName()
            << " <asset_bundle_path> <flutter_flags>`" << std::endl
            << std::endl;
  std::cerr << R"~(
This utility runs an instance of a Flutter application and renders using
Wayland core protocols.

The Flutter tools can be obtained at https://flutter.io/

asset_bundle_path: The Flutter application code needs to be snapshotted using
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

static FlutterApplication* application;

class InputEventListener : public WaylandDisplay::InputDelegate {
  private:
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t status = 0;

    void OnDisplayPointerEnter(uint32_t x, uint32_t y) {
      this->x = x;
      this->y = y;
    }

    void OnDisplayPointerLeave() {
      this->x = 0;
      this->y = 0;
    }

    void OnDisplayPointerMotion(uint32_t x, uint32_t y) {
      this->x = x;
      this->y = y;
      application->SendPointerEvent(this->status, this->x, this->y);
    }

    void OnDisplayPointerButton(uint32_t button, uint32_t status) {
      this->status = status;
      application->SendPointerEvent(this->status, this->x, this->y);
    }

    void OnDisplayPointerAxis(uint32_t axis, uint32_t value) {
      // Nothing to do.
    }
};

static bool Main(std::vector<std::string> args) {
  if (args.size() == 0) {
    std::cerr << "   <Invalid Arguments>   " << std::endl;
    PrintUsage();
    return false;
  }

  const auto asset_bundle_path = args[0];

  if (!FlutterAssetBundleIsValid(asset_bundle_path)) {
    std::cerr << "   <Invalid Flutter Asset Bundle>   " << std::endl;
    PrintUsage();
    return false;
  }

  const size_t kWidth = 800;
  const size_t kHeight = 600;

  for (const auto& arg : args) {
    FLWAY_LOG << "Arg: " << arg << std::endl;
  }

  InputEventListener inputEventListener;
  WaylandDisplay display(kWidth, kHeight, inputEventListener);

  if (!display.IsValid()) {
    FLWAY_ERROR << "Wayland display was not valid." << std::endl;
    return false;
  }

  application = new FlutterApplication(asset_bundle_path, args, display);
  if (!application->IsValid()) {
    FLWAY_ERROR << "Flutter application was not valid." << std::endl;
    return false;
  }

  if (!application->SetWindowSize(kWidth, kHeight)) {
    FLWAY_ERROR << "Could not update Flutter application size." << std::endl;
    return false;
  }

  display.Run();

  delete application;

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
