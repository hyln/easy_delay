#include "clock_protocol.hpp"
#include "clock_utils.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {

struct Sample { std::int64_t rtt_ns; std::int64_t offset_ns; };

std::int64_t median_offset(std::vector<Sample> samples) {
  std::sort(samples.begin(), samples.end(), [](const auto& a, const auto& b) {
    return a.offset_ns < b.offset_ns;
  });
  return samples[samples.size() / 2].offset_ns;
}

double milliseconds(std::int64_t nanoseconds) {
  return static_cast<double>(nanoseconds) / 1000000.0;
}

}  // namespace

int main(int argc, char** argv) {
  // Stage 1: Read values supplied by the Python orchestration layer.
  if (argc != 6) {
    std::cerr << "usage: easy-delay-client HOST PORT SESSION SAMPLES INTERVAL_MS\n";
    return 2;
  }
  const char* host = argv[1];
  const char* port = argv[2];
  const std::uint64_t session = std::strtoull(argv[3], nullptr, 16);
  const int requested = std::max(10, std::atoi(argv[4]));
  const int interval_ms = std::max(1, std::atoi(argv[5]));

  // Stage 2: Resolve the target and create the connected UDP socket.
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  addrinfo* addresses = nullptr;
  if (::getaddrinfo(host, port, &hints, &addresses) != 0) return 3;
  int fd = -1;
  for (addrinfo* current = addresses; current; current = current->ai_next) {
    fd = ::socket(current->ai_family, current->ai_socktype, current->ai_protocol);
    if (fd >= 0 && ::connect(fd, current->ai_addr, current->ai_addrlen) == 0) break;
    if (fd >= 0) ::close(fd);
    fd = -1;
  }
  ::freeaddrinfo(addresses);
  if (fd < 0) return 4;

  // Stage 3: Collect independent four-timestamp exchanges.
  std::vector<Sample> samples;
  for (int sequence = 0; sequence < requested; ++sequence) {
    easy_delay::Packet request{};
    request.type = easy_delay::PacketType::request;
    request.session = session;
    request.sequence = static_cast<std::uint32_t>(sequence);
    request.t1_ns = easy_delay::now_ns(CLOCK_REALTIME);
    const auto bytes = easy_delay::encode(request);
    if (::send(fd, bytes.data(), bytes.size(), 0) != static_cast<ssize_t>(bytes.size())) continue;

    pollfd event{fd, POLLIN, 0};
    if (::poll(&event, 1, 500) <= 0) continue;
    std::uint8_t response_bytes[easy_delay::kPacketSize]{};
    const ssize_t size = ::recv(fd, response_bytes, sizeof(response_bytes), 0);
    const std::int64_t t4_ns = easy_delay::now_ns(CLOCK_REALTIME);
    easy_delay::Packet response{};
    if (size < 0 || !easy_delay::decode(response_bytes, static_cast<std::size_t>(size), response) ||
        response.type != easy_delay::PacketType::response || response.session != session ||
        response.sequence != static_cast<std::uint32_t>(sequence)) continue;
    const std::int64_t rtt = (t4_ns - response.t1_ns) - (response.t3_ns - response.t2_ns);
    const std::int64_t offset = ((response.t2_ns - response.t1_ns) +
                                 (response.t3_ns - t4_ns)) / 2;
    if (rtt >= 0) samples.push_back({rtt, offset});
    timespec pause{0, interval_ms * 1000000L};
    ::nanosleep(&pause, nullptr);
  }
  ::close(fd);

  // Stage 4: Use the lowest-RTT samples and emit one machine-readable result.
  if (samples.size() < 5) {
    std::cout << "{\"result\":\"error\",\"error\":\"insufficient_samples\"," 
                 "\"valid_count\":" << samples.size() << "}\n";
    return 5;
  }
  std::sort(samples.begin(), samples.end(), [](const auto& a, const auto& b) {
    return a.rtt_ns < b.rtt_ns;
  });
  const std::size_t selected_count = std::min<std::size_t>(10, samples.size());
  std::vector<Sample> selected(samples.begin(), samples.begin() + selected_count);
  const std::int64_t offset = median_offset(selected);
  auto [minimum, maximum] = std::minmax_element(selected.begin(), selected.end(),
      [](const auto& a, const auto& b) { return a.offset_ns < b.offset_ns; });
  std::cout << "{\"result\":\"measured\",\"requested_count\":" << requested
            << ",\"valid_count\":" << samples.size()
            << ",\"selected_count\":" << selected_count
            << ",\"minimum_rtt_ms\":" << milliseconds(samples.front().rtt_ns)
            << ",\"offset_ms\":" << milliseconds(offset)
            << ",\"offset_spread_ms\":" << milliseconds(maximum->offset_ns - minimum->offset_ns)
            << "}\n";
  return 0;
}

