// Copyright 2020 Joel Winarske. All rights reserved.
// Copyright 2018 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "utils.h"

#include <unistd.h>
#include <sstream>

namespace flutter {

static std::string GetExecutablePath() {
  char executable_path[1024] = {0};
  std::stringstream stream;
  stream << "/proc/" << getpid() << "/exe";
  auto path = stream.str();
  auto executable_path_size =
      ::readlink(path.c_str(), executable_path, sizeof(executable_path));
  if (executable_path_size <= 0) {
    return "";
  }
  return std::string{executable_path,
                     static_cast<size_t>(executable_path_size)};
}

std::string GetExecutableName() {
  auto path_string = GetExecutablePath();
  auto found = path_string.find_last_of('/');
  if (found == std::string::npos) {
    return "";
  }
  return path_string.substr(found + 1);
}

std::string GetExecutableDirectory() {
  auto path_string = GetExecutablePath();
  auto found = path_string.find_last_of('/');
  if (found == std::string::npos) {
    return "";
  }
  return path_string.substr(0, found + 1);
}

std::string GetAotFilepath(const std::string& path) {
  return path + std::string{"/"} + std::string{kAotFileName};
}

bool FileExistsAtPath(const std::string& path) {
  return ::access(path.c_str(), R_OK) == 0;
}

bool FlutterAotPresent(const std::string& path) {
  if (!FileExistsAtPath(path)) {
    FLWAY_ERROR << "Asset directory does not exist." << std::endl;
    return false;
  }

  if (!FileExistsAtPath(path + std::string{"/"} + std::string{kAotFileName})) {
    return false;
  }

  return true;
}

bool FlutterAssetsPathIsValid(const std::string& path) {
  if (!FileExistsAtPath(path)) {
    FLWAY_ERROR << "Asset directory does not exist." << std::endl;
    return false;
  }

  if (!FileExistsAtPath(path + std::string{"/"} + std::string{kAotFileName})) {
    if (!FileExistsAtPath(path + std::string{"/"} + std::string{kKernelBlobFileName})) {
      FLWAY_ERROR << "Kernel blob does not exist." << std::endl;
      return false;
    }
  }

  return true;
}

}  // namespace flutter
