Flutter Wayland
============

A Flutter Embedder that talks to Wayland.

Setup Instructions
------------------

* Install the following packages (on Debian Stretch): `weston`, `libwayland-dev`, `cmake` and `ninja`.
* From the source root `mkdir build` and move into the directory.
* `cmake -G Ninja ../`. This should check you development environment for required packages, download the Flutter engine artifacts and unpack the same in the build directory.
* `ninja` to build the embedder.
* Run the embedder using `./flutter_wayland`. Make sure `weston` is running.
