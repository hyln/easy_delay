#pragma once

#include <array>
#include <cstdint>
#include <cstring>

namespace easy_delay {

constexpr std::uint32_t kMagic = 0x45444c59;  // EDLY
constexpr std::uint8_t kVersion = 1;
constexpr std::size_t kPacketSize = 48;

enum class PacketType : std::uint8_t { request = 1, response = 2 };

struct Packet {
  PacketType type{PacketType::request};
  std::uint64_t session{};
  std::uint32_t sequence{};
  std::int64_t t1_ns{};
  std::int64_t t2_ns{};
  std::int64_t t3_ns{};
};

inline void put_u32(std::uint8_t* out, std::uint32_t value) {
  for (int i = 3; i >= 0; --i) *out++ = static_cast<std::uint8_t>(value >> (i * 8));
}

inline void put_u64(std::uint8_t* out, std::uint64_t value) {
  for (int i = 7; i >= 0; --i) *out++ = static_cast<std::uint8_t>(value >> (i * 8));
}

inline std::uint32_t get_u32(const std::uint8_t* in) {
  std::uint32_t value = 0;
  for (int i = 0; i < 4; ++i) value = (value << 8) | in[i];
  return value;
}

inline std::uint64_t get_u64(const std::uint8_t* in) {
  std::uint64_t value = 0;
  for (int i = 0; i < 8; ++i) value = (value << 8) | in[i];
  return value;
}

inline std::array<std::uint8_t, kPacketSize> encode(const Packet& packet) {
  std::array<std::uint8_t, kPacketSize> out{};
  put_u32(out.data(), kMagic);
  out[4] = kVersion;
  out[5] = static_cast<std::uint8_t>(packet.type);
  put_u64(out.data() + 8, packet.session);
  put_u32(out.data() + 16, packet.sequence);
  put_u64(out.data() + 24, static_cast<std::uint64_t>(packet.t1_ns));
  put_u64(out.data() + 32, static_cast<std::uint64_t>(packet.t2_ns));
  put_u64(out.data() + 40, static_cast<std::uint64_t>(packet.t3_ns));
  return out;
}

inline bool decode(const std::uint8_t* data, std::size_t size, Packet& packet) {
  if (size != kPacketSize || get_u32(data) != kMagic || data[4] != kVersion) return false;
  if (data[5] != static_cast<std::uint8_t>(PacketType::request) &&
      data[5] != static_cast<std::uint8_t>(PacketType::response)) return false;
  packet.type = static_cast<PacketType>(data[5]);
  packet.session = get_u64(data + 8);
  packet.sequence = get_u32(data + 16);
  packet.t1_ns = static_cast<std::int64_t>(get_u64(data + 24));
  packet.t2_ns = static_cast<std::int64_t>(get_u64(data + 32));
  packet.t3_ns = static_cast<std::int64_t>(get_u64(data + 40));
  return true;
}

}  // namespace easy_delay

