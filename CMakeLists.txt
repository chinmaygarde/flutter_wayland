# Copyright 2018 The Flutter Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.5.2)

project(flutter_wayland)

if(NOT FLUTTER_ENGINE_LIBRARY AND NOT CMAKE_CROSSCOMPILING)

  if(NOT FLUTTER_ENGINE_SHA)

    if(NOT CHANNEL)
        set(CHANNEL "stable" CACHE STRING "Choose the channel, options are: master, dev, beta, stable" FORCE)
        message(STATUS "Flutter Channel not set, defaulting to stable")
    endif()

    message(STATUS "Flutter Channel ........ ${CHANNEL}")

    include(FetchContent)

    FetchContent_Declare(engine-version
      URL https://raw.githubusercontent.com/flutter/flutter/${CHANNEL}/bin/internal/engine.version
      DOWNLOAD_NAME engine.version
      DOWNLOAD_NO_EXTRACT TRUE
      DOWNLOAD_DIR ${CMAKE_BINARY_DIR}
    )

    FetchContent_GetProperties(engine-version)
    if(NOT engine-version_POPULATED)
        FetchContent_Populate(engine-version)
        file(READ ${CMAKE_BINARY_DIR}/engine.version FLUTTER_ENGINE_SHA)
        string(REPLACE "\n" "" FLUTTER_ENGINE_SHA ${FLUTTER_ENGINE_SHA})
    else()
        MESSAGE(FATAL "Unable to determine engine-version, please override FLUTTER_ENGINE_SHA")
    endif()

  endif()

  message(STATUS "Engine SHA1 ............ ${FLUTTER_ENGINE_SHA}")

  # Download and setup the Flutter Engine.

  set(FLUTTER_EMBEDDER_ARTIFACTS_ZIP ${CMAKE_BINARY_DIR}/flutter_embedder_${FLUTTER_ENGINE_SHA}.zip)
  set(FLUTTER_BUCKET_BASE "https://storage.googleapis.com/flutter_infra/flutter")

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

  set(FLUTTER_ENGINE_LIBRARY ${CMAKE_BINARY_DIR}/libflutter_engine.so)
else()
  message(STATUS "Engine ................. ${FLUTTER_ENGINE_LIBRARY}")
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

include(FindPkgConfig)
pkg_check_modules(WAYLANDPP_CLIENT REQUIRED "wayland-client++>=0.2.7")
pkg_check_modules(WAYLANDPP_CURSOR REQUIRED "wayland-cursor++>=0.2.7")
pkg_check_modules(WAYLANDPP_EGL REQUIRED "wayland-egl++>=0.2.7")
pkg_check_modules(WAYLANDPP_CLIENT_EXTRA REQUIRED "wayland-client-extra++>=0.2.7")
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(XKBCOMMON REQUIRED xkbcommon)
pkg_check_modules(RAPIDJSON REQUIRED "RapidJSON>=1.1.0")



set(FLUTTER_WAYLAND_SRC
  flutter/standard_codec.cc
  src/wayland_display.cc
  src/keyboard.cc
  src/platform_channel.cc
  src/utils.cc
  src/main.cc
)

link_directories(${CMAKE_BINARY_DIR})

add_executable(flutter_wayland ${FLUTTER_WAYLAND_SRC})

target_link_libraries(flutter_wayland dl
  ${FLUTTER_ENGINE_LIBRARY}
  ${WAYLANDPP_CLIENT_LIBRARIES}
  ${WAYLANDPP_EGL_LIBRARIES}
  ${WAYLANDPP_CURSOR_LIBRARIES}
  ${WAYLANDPP_CLIENT_EXTRA_LIBRARIES}
  ${EGL_LDFLAGS}
  ${XKBCOMMON_LIBRARIES}
  ${RAPIDJSON_LIBRARIES}
)

target_include_directories(flutter_wayland
  PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}
  ${WAYLANDPP_CLIENT_INCLUDE_DIRS}
  ${WAYLANDPP_CURSOR_INCLUDE_DIRS}
  ${WAYLANDPP_EGL_INCLUDE_DIRS}
  ${WAYLANDPP_CLIENT_EXTRA_INCLUDE_DIRS}
  ${EGL_INCLUDE_DIRS}
  ${XKBCOMMON_INCLUDE_DIRS}
  ${RAPIDJSON_INCLUDE_DIRS}
)

target_compile_options(flutter_wayland PUBLIC ${EGL_CFLAGS})

install(TARGETS flutter_wayland RUNTIME DESTINATION bin)
