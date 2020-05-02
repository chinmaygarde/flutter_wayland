// Copyright 2020 Joel Winarske. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

namespace flutter {

// Filenames
constexpr char kAotFileName[] = "libaot.so";
constexpr char kICUDataFileName[] = "icudtl.dat";
constexpr char kKernelBlobFileName[] = "kernel_blob.bin";

// Symbols
constexpr char kDartVmSnapshotInstructions[] = "_kDartVmSnapshotInstructions";
constexpr char kDartIsolateSnapshotInstructions[] = "_kDartIsolateSnapshotInstructions";
constexpr char kDartVmSnapshotData[] = "_kDartVmSnapshotData";
constexpr char kDartIsolateSnapshotData[] = "_kDartIsolateSnapshotData";

}  // namespace flutter
