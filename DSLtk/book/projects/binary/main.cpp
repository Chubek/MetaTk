#include "DSLtk.hpp"
#include <iostream>

struct Frame { std::string name; bool operator==(const Frame&) const = default; };
std::span<const std::byte> as_bytes(std::string_view text) {
  return std::as_bytes(std::span(text.data(), text.size()));
}
std::uint8_t checksum(std::string_view text) {
  unsigned sum = 0;
  for (unsigned char c : text) sum += c;
  return static_cast<std::uint8_t>(sum & 255);
}
struct Codec {
  void serialize(const Frame& frame, dsl::BinaryStream& out) {
    if (frame.name.size() > 64 || !dsl::StaticRegex<"[a-z][a-z0-9_]*">::matches(frame.name))
      throw std::invalid_argument("invalid frame name");
    out.write(as_bytes("DT"));
    out.write_integer<std::uint8_t>(1);
    out.write_integer<std::uint16_t>(static_cast<std::uint16_t>(frame.name.size()), dsl::ByteOrder::Big);
    out.write(as_bytes(frame.name));
    out.write_integer<std::uint8_t>(checksum(frame.name));
  }
  template<class T> T deserialize(dsl::BinaryStream& in) {
    static_assert(std::same_as<T, Frame>);
    auto checkpoint = in.pos;
    try {
      auto magic = in.read(2);
      if (magic[0] != std::byte{'D'} || magic[1] != std::byte{'T'}
          || in.read_integer<std::uint8_t>() != 1)
        throw std::invalid_argument("invalid header");
      auto length = in.read_integer<std::uint16_t>(dsl::ByteOrder::Big);
      if (length > 64) throw std::invalid_argument("oversized frame");
      auto payload = in.read(length);
      std::string name(reinterpret_cast<const char*>(payload.data()), payload.size());
      if (!dsl::StaticRegex<"[a-z][a-z0-9_]*">::matches(name)
          || in.read_integer<std::uint8_t>() != checksum(name))
        throw std::invalid_argument("invalid payload or checksum");
      return {std::move(name)};
    } catch (...) { in.pos = checkpoint; throw; }
  }
};
struct ShortBuffer {
  std::vector<std::byte> data;
  std::size_t pos = 0;
  std::size_t read(std::span<std::byte> output) {
    auto n = std::min({output.size(), data.size() - pos, std::size_t{2}});
    std::copy_n(data.begin() + pos, n, output.begin());
    pos += n;
    return n;
  }
  std::size_t write(std::span<const std::byte> input) {
    auto n = std::min(input.size(), std::size_t{3});
    data.insert(data.end(), input.begin(), input.begin() + n);
    return n;
  }
};
int main() {
  Codec codec;
  dsl::Serializer serializer(codec);
  dsl::Deserializer deserializer(codec);
  dsl::BinaryStream encoded;
  serializer.serialize(Frame{"sensor_7"}, encoded);
  ShortBuffer backend;
  dsl::IO io(backend);
  encoded.save(io);
  auto received = dsl::BinaryStream::load(io, 1024);
  auto frame = deserializer.deserialize<Frame>(received);
  assert(frame == Frame{"sensor_7"} && received.eof());
  for (std::size_t length = 0; length < encoded.bytes.size(); ++length) {
    dsl::BinaryStream truncated(std::span<const std::byte>(encoded.bytes).first(length));
    bool rejected = false;
    try { (void)deserializer.deserialize<Frame>(truncated); }
    catch (const std::exception&) { rejected = true; }
    assert(rejected && truncated.pos == 0);
  }
  auto corrupted = encoded;
  corrupted.bytes.back() ^= std::byte{1};
  bool rejected = false;
  try { (void)deserializer.deserialize<Frame>(corrupted); }
  catch (const std::invalid_argument&) { rejected = true; }
  assert(rejected && corrupted.pos == 0);
  std::cout << "frame=" << frame.name << " bytes=" << encoded.bytes.size()
            << " partial-I/O round trip OK\n";
}
