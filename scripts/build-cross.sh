#!/usr/bin/env sh
set -eu

# Stage 1: Keep the supported target matrix explicit and reproducible.
mkdir -p prebuilt

# Stage 2: Build fully static musl servers; toolchains must already be installed.
x86_64-linux-musl-g++ -O2 -std=c++17 -static -fno-exceptions -fno-rtti \
  cpp/udp_clock_server.cpp -o prebuilt/easy-delay-server-amd64
aarch64-linux-musl-g++ -O2 -std=c++17 -static -fno-exceptions -fno-rtti \
  cpp/udp_clock_server.cpp -o prebuilt/easy-delay-server-arm64
arm-linux-musleabihf-g++ -O2 -std=c++17 -static -fno-exceptions -fno-rtti \
  -march=armv7-a cpp/udp_clock_server.cpp -o prebuilt/easy-delay-server-armv7

# Stage 3: Verify that no target libc is required.
file prebuilt/easy-delay-server-amd64 prebuilt/easy-delay-server-arm64 \
  prebuilt/easy-delay-server-armv7
