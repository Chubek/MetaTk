#pragma once
#include "MetaTeX-Reader.h"
#include <chrono>
#include <mqueue.h>
#include <vector>
namespace metatex::io {
// Priority-zero messages form one byte stream, ended by a zero-length message.
// Use one producer/consumer per stream. Partial reads are retained.
class MSGQueueReader final : public Reader {
public:
    explicit MSGQueueReader(const std::string& name,
        std::chrono::milliseconds timeout = std::chrono::seconds(30));
    ~MSGQueueReader() override;
    MSGQueueReader(const MSGQueueReader&) = delete;
    MSGQueueReader& operator=(const MSGQueueReader&) = delete;
    std::size_t read(char* destination, std::size_t capacity) override;
private:
    mqd_t queue_ = static_cast<mqd_t>(-1);
    std::chrono::milliseconds timeout_;
    std::vector<char> message_;
    std::size_t offset_ = 0, size_ = 0;
    bool eof_ = false;
};
}
