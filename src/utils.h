// Copyright 2020 Joel Winarske. All rights reserved.
// Copyright 2018 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "macros.h"
#include "constants.h"

namespace flutter {

std::string GetExecutableName();

std::string GetExecutableDirectory();

std::string GetAotFilepath(const std::string& path);

bool FileExistsAtPath(const std::string& path);

bool FlutterAotPresent(const std::string& path);

bool FlutterAssetsPathIsValid(const std::string& path);

}  // namespace flutter
