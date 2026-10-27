#pragma once
#include "MetaTeX-Reader.h"
#include <chrono>
namespace metatex::io {
// Returned descriptors are caller-owned. DNS resolution is synchronous;
// timeout bounds connection attempts after resolution.
int connectTCP(const std::string& host, const std::string& service,
    std::chrono::milliseconds timeout = std::chrono::seconds(30));
int connectUnix(const std::filesystem::path& path,
    std::chrono::milliseconds timeout = std::chrono::seconds(30));
class SocketReader final : public Reader {
public:
    explicit SocketReader(int descriptor, bool own = false,
        std::chrono::milliseconds timeout = std::chrono::seconds(30));
    ~SocketReader() override;
    SocketReader(const SocketReader&) = delete;
    SocketReader& operator=(const SocketReader&) = delete;
    std::size_t read(char* destination, std::size_t capacity) override;
private:
    int fd_;
    bool own_;
    std::chrono::milliseconds timeout_;
};
}
