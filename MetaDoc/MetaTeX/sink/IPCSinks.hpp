#pragma once
#include "MetaTeX-Sink.h"
#include <chrono>
#include <mqueue.h>
namespace metatex::io {
class FIFOSink final : public DescriptorSink {
public:
    explicit FIFOSink(const std::filesystem::path& path);
    void write(std::string_view bytes) override;
};
// Creates a new name exclusively on flush. Consumers may open it after flush
// returns. Caller owns shm_unlink lifetime; destructor does not publish.
class SharedMemorySink final : public Sink {
public:
    explicit SharedMemorySink(std::string name, std::size_t maxBytes = 16u * 1024u * 1024u);
    void write(std::string_view bytes) override;
    void flush() override;
private:
    std::string name_, bytes_;
    std::size_t limit_;
    bool published_ = false;
};
// create=true exclusively creates a queue, otherwise opens an existing queue.
// Caller owns mq_unlink lifetime. finish() sends EOF; flush() does not.
class MSGQueueSink final : public Sink {
public:
    explicit MSGQueueSink(const std::string& name, bool create = false,
        std::chrono::milliseconds timeout = std::chrono::seconds(30), long messageSize = 8192);
    ~MSGQueueSink() override;
    MSGQueueSink(const MSGQueueSink&) = delete;
    MSGQueueSink& operator=(const MSGQueueSink&) = delete;
    void write(std::string_view bytes) override;
    void flush() override {}
    void finish();
private:
    void send(std::string_view bytes, const timespec& deadline);
    mqd_t queue_ = static_cast<mqd_t>(-1);
    std::size_t messageSize_ = 0;
    std::chrono::milliseconds timeout_;
    bool finished_ = false;
};
}
