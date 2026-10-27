#pragma once
// Short-write-safe, binary output sinks. Paths are not shell-interpreted.
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace metatex::io {
class Sink {
public:
    virtual ~Sink() = default;
    virtual void write(std::string_view bytes) = 0;
    virtual void flush() = 0;
};
class FileSink final : public Sink {
public:
    explicit FileSink(const std::filesystem::path& path);
    void write(std::string_view bytes) override;
    void flush() override;
private:
    std::ofstream stream_;
};
class MemorySink final : public Sink {
public:
    void write(std::string_view bytes) override { result_.append(bytes); }
    void flush() override {}
    const std::string& result() const { return result_; }
    std::string take() { return std::move(result_); }
private:
    std::string result_;
};
class DescriptorSink : public Sink {
public:
    explicit DescriptorSink(int descriptor, bool own = false);
    ~DescriptorSink() override;
    DescriptorSink(const DescriptorSink&) = delete;
    DescriptorSink& operator=(const DescriptorSink&) = delete;
    void write(std::string_view bytes) override;
    void flush() override;
private:
    int fd_;
    bool own_;
};
} // namespace metatex::io
