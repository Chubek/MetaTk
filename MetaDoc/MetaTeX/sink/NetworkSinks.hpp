#pragma once
#include "MetaTeX-Sink.h"
#include <chrono>
namespace metatex::io {
class SocketSink final : public Sink {
public:
    explicit SocketSink(int descriptor, bool own = false,
        std::chrono::milliseconds timeout = std::chrono::seconds(30));
    ~SocketSink() override;
    SocketSink(const SocketSink&) = delete;
    SocketSink& operator=(const SocketSink&) = delete;
    void write(std::string_view bytes) override;
    void flush() override {} // Writes are unbuffered.
    void finish(); // Half-close write direction, preserving read direction.
private:
    int fd_;
    bool own_, finished_ = false;
    std::chrono::milliseconds timeout_;
};
}
