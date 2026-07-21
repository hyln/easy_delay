#pragma once

#include <cstdint>
#include <ctime>

namespace easy_delay {

inline std::int64_t now_ns(clockid_t clock_id) {
  timespec value{};
  if (::clock_gettime(clock_id, &value) != 0) return -1;
  return static_cast<std::int64_t>(value.tv_sec) * 1000000000LL + value.tv_nsec;
}

}  // namespace easy_delay

