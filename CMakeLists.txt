# Copyright 2018 The Flutter Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.5.2)

project(flutter_wayland)

set(FLUTTER_ENGINE_SHA b9523318caa1a99ffde8adaf331212eb879cabc9)

set(FLUTTER_EMBEDDER_ARTIFACTS_ZIP ${CMAKE_BINARY_DIR}/flutter_embedder_${FLUTTER_ENGINE_SHA}.zip)
set(FLUTTER_ARTIFACTS_ZIP          ${CMAKE_BINARY_DIR}/flutter_artifact_${FLUTTER_ENGINE_SHA}.zip)
set(FLUTTER_BUCKET_BASE "https://storage.googleapis.com/flutter_infra/flutter")

# Download and setup the Flutter Engine.
if(NOT EXISTS ${FLUTTER_EMBEDDER_ARTIFACTS_ZIP})
  file(DOWNLOAD
    ${FLUTTER_BUCKET_BASE}/${FLUTTER_ENGINE_SHA}/linux-x64/linux-x64-embedder
    ${FLUTTER_EMBEDDER_ARTIFACTS_ZIP}
    SHOW_PROGRESS
  )
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar xzf ${FLUTTER_EMBEDDER_ARTIFACTS_ZIP}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
endif()

if(NOT EXISTS ${FLUTTER_ARTIFACTS_ZIP})
  file(DOWNLOAD
    ${FLUTTER_BUCKET_BASE}/${FLUTTER_ENGINE_SHA}/linux-x64/artifacts.zip
    ${FLUTTER_ARTIFACTS_ZIP}
    SHOW_PROGRESS
  )
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar xzf ${FLUTTER_ARTIFACTS_ZIP}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
endif()

find_package(PkgConfig)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
pkg_check_modules(WAYLAND_EGL    REQUIRED wayland-egl)
pkg_check_modules(EGL            REQUIRED egl)

# Executable
file(GLOB_RECURSE FLUTTER_WAYLAND_SRC
  "src/*.cc"
  "src/*.h"
)

link_directories(${CMAKE_BINARY_DIR})

add_executable(flutter_wayland ${FLUTTER_WAYLAND_SRC})

target_link_libraries(flutter_wayland
  ${WAYLAND_CLIENT_LIBRARIES}
  ${WAYLAND_EGL_LIBRARIES}
  ${EGL_LIBRARIES}
  flutter_engine
)

target_include_directories(flutter_wayland
  PRIVATE
  ${WAYLAND_CLIENT_INCLUDE_DIRS}
  ${WAYLAND_EGL_INCLUDE_DIRS}
  ${EGL_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR}
)
