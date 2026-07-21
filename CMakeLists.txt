cmake_minimum_required(VERSION 3.18)
project(easy_delay LANGUAGES CXX)

option(EASY_DELAY_REQUIRE_ARM64_SERVER
       "Require and bundle an ARM64 target server" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(easy-delay-client cpp/udp_clock_client.cpp)
target_compile_options(easy-delay-client PRIVATE -O2 -Wall -Wextra -Wpedantic)

install(TARGETS easy-delay-client RUNTIME DESTINATION easy_delay/bin)

# Build a libc-independent server for targets matching the build machine.
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" SYSTEM_PROCESSOR_LOWER)
if(SYSTEM_PROCESSOR_LOWER MATCHES "^(x86_64|amd64)$")
  set(SERVER_ARCH "amd64")
elseif(SYSTEM_PROCESSOR_LOWER MATCHES "^(aarch64|arm64)$")
  set(SERVER_ARCH "arm64")
elseif(SYSTEM_PROCESSOR_LOWER MATCHES "^(armv7|armv7l)$")
  set(SERVER_ARCH "armv7")
else()
  message(FATAL_ERROR "Unsupported server build architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

add_executable(easy-delay-server cpp/udp_clock_server.cpp)
set_target_properties(easy-delay-server PROPERTIES
  OUTPUT_NAME "easy-delay-server-${SERVER_ARCH}")
target_compile_options(easy-delay-server PRIVATE
  -O2 -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti)
target_link_options(easy-delay-server PRIVATE -static)
install(TARGETS easy-delay-server RUNTIME DESTINATION easy_delay/bin)

# Cross-compile the ARM64 target agent during the same x64 wheel build. The
# wheel must never be emitted without it when ARM64 support is required.
if(EASY_DELAY_REQUIRE_ARM64_SERVER AND NOT SERVER_ARCH STREQUAL "arm64")
  find_program(ARM64_CXX
    NAMES aarch64-linux-musl-g++ aarch64-linux-gnu-g++)
  if(NOT ARM64_CXX)
    message(FATAL_ERROR
      "ARM64 cross compiler not found. Install g++-aarch64-linux-gnu or "
      "provide aarch64-linux-musl-g++; refusing to build an incomplete wheel.")
  endif()

  set(ARM64_SERVER "${CMAKE_BINARY_DIR}/easy-delay-server-arm64")
  add_custom_command(
    OUTPUT "${ARM64_SERVER}"
    COMMAND "${ARM64_CXX}" -O2 -std=c++17 -static -fno-exceptions -fno-rtti
            -I "${CMAKE_SOURCE_DIR}/cpp"
            "${CMAKE_SOURCE_DIR}/cpp/udp_clock_server.cpp"
            -o "${ARM64_SERVER}"
    DEPENDS cpp/udp_clock_server.cpp cpp/clock_protocol.hpp cpp/clock_utils.hpp
    VERBATIM
    COMMENT "Cross-compiling static ARM64 target server")
  add_custom_target(easy-delay-server-arm64 ALL DEPENDS "${ARM64_SERVER}")
  install(PROGRAMS "${ARM64_SERVER}" DESTINATION easy_delay/bin)
endif()

# Additional cross-compiled static servers are included in the same wheel.
file(GLOB CROSS_SERVERS "${CMAKE_SOURCE_DIR}/prebuilt/easy-delay-server-*")
if(CROSS_SERVERS)
  install(PROGRAMS ${CROSS_SERVERS} DESTINATION easy_delay/bin)
endif()
