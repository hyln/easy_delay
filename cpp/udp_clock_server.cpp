#include "clock_protocol.hpp"
#include "clock_utils.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

int parse_port(const char* text) {
  const long value = std::strtol(text, nullptr, 10);
  return value >= 1024 && value <= 65535 ? static_cast<int>(value) : -1;
}

}  // namespace

int main(int argc, char** argv) {
  // Stage 1: Load the small, internally supplied runtime configuration.
  if (argc != 3) {
    std::cerr << "usage: easy-delay-server PORT SESSION\n";
    return 2;
  }
  const int port = parse_port(argv[1]);
  const std::uint64_t session = std::strtoull(argv[2], nullptr, 16);
  if (port < 0 || session == 0) return 2;

  // Stage 2: Bind one short-lived UDP service for this measurement session.
  const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) return 3;
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(static_cast<std::uint16_t>(port));
  if (::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    std::cerr << "bind: " << std::strerror(errno) << '\n';
    ::close(fd);
    return 4;
  }
  std::cout << "READY" << std::endl;

  // Stage 3: Timestamp and answer fixed-size authenticated requests.
  int idle_rounds = 0;
  while (idle_rounds < 30) {
    pollfd event{fd, POLLIN, 0};
    const int ready = ::poll(&event, 1, 1000);
    if (ready == 0) { ++idle_rounds; continue; }
    if (ready < 0) { if (errno == EINTR) continue; break; }

    std::uint8_t buffer[easy_delay::kPacketSize]{};
    sockaddr_storage peer{};
    socklen_t peer_size = sizeof(peer);
    const ssize_t size = ::recvfrom(fd, buffer, sizeof(buffer), 0,
                                    reinterpret_cast<sockaddr*>(&peer), &peer_size);
    const std::int64_t t2_ns = easy_delay::now_ns(CLOCK_REALTIME);
    easy_delay::Packet packet{};
    if (size < 0 || !easy_delay::decode(buffer, static_cast<std::size_t>(size), packet) ||
        packet.type != easy_delay::PacketType::request || packet.session != session) continue;
    idle_rounds = 0;
    packet.type = easy_delay::PacketType::response;
    packet.t2_ns = t2_ns;
    packet.t3_ns = easy_delay::now_ns(CLOCK_REALTIME);
    const auto response = easy_delay::encode(packet);
    ::sendto(fd, response.data(), response.size(), 0,
             reinterpret_cast<sockaddr*>(&peer), peer_size);
  }

  // Stage 4: Exit automatically so an interrupted orchestrator leaves no daemon.
  ::close(fd);
  return 0;
}

