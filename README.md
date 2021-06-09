# Flutter Wayland

A Flutter Embedder that talks to Wayland

![Running in Weston](assets/ubuntu_wayland_18.0.4.png)

#### Build Setup Instructions

* Ubuntu 18/20 deps: `sudo apt-get install cmake lib-wayland++ libgl1-mesa-dev libegl1-mesa-dev libgles2-mesa-dev libxkbcommon-dev rapidjson-dev clang`
* Fedora 33/34 deps: `sudo dnf install wayland-devel libxkbcommon-devel rapidjson-devel pugixml-devel waylandpp-devel clang`

```
mkdir build && cd build
CXX=/usr/bin/clang++ CC=/usr/bin/clang cmake ..
make -j VERBOSE=1
```

* Run the embedder using `./flutter_wayland`. 

#### Enable Wayland on Ubuntu 16/18

* log out
* select gear icon
* select `Ubuntu on Wayland`
* login as usual

## Running Flutter Application

```
Flutter Wayland Embedder

========================
Usage: `flutter_wayland <asset_path> <flutter_flags>`


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
```
