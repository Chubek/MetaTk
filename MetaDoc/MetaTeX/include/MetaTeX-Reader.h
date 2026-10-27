#pragma once
// Interchangeable bounded binary input sources for the scanner and toolchain.
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <istream>
#include <string>
#include <string_view>

namespace metatex::io {
class Reader {
public:
    virtual ~Reader() = default;
    // Return 0 only on EOF. Throw std::runtime_error on I/O error.
    virtual std::size_t read(char* destination, std::size_t capacity) = 0;
    std::string readAll(std::size_t maxBytes = 16u * 1024u * 1024u);
};

class FileReader final : public Reader {
public:
    explicit FileReader(const std::filesystem::path& path);
    std::size_t read(char* destination, std::size_t capacity) override;
private:
    std::ifstream stream_;
};

class StreamReader final : public Reader {
public:
    explicit StreamReader(std::istream& stream) : stream_(stream) {}
    std::size_t read(char* destination, std::size_t capacity) override;
private:
    std::istream& stream_;
};

class MemoryReader final : public Reader {
public:
    explicit MemoryReader(std::string_view data) : data_(data) {}
    std::size_t read(char* destination, std::size_t capacity) override;
private:
    std::string_view data_;
    std::size_t offset_ = 0;
};

// A borrowed or owned POSIX descriptor, for pipes, sockets, and FIFOs.
class DescriptorReader : public Reader {
public:
    explicit DescriptorReader(int descriptor, bool own = false);
    ~DescriptorReader() override;
    DescriptorReader(const DescriptorReader&) = delete;
    DescriptorReader& operator=(const DescriptorReader&) = delete;
    DescriptorReader(DescriptorReader&&) = delete;
    std::size_t read(char* destination, std::size_t capacity) override;
private:
    int fd_;
    bool own_;
};

// Maps a regular file, handles empty files, and copies out only requested bytes.
class MappedFileReader final : public Reader {
public:
    explicit MappedFileReader(const std::filesystem::path& path);
    ~MappedFileReader() override;
    MappedFileReader(const MappedFileReader&) = delete;
    MappedFileReader& operator=(const MappedFileReader&) = delete;
    std::size_t read(char* destination, std::size_t capacity) override;
private:
    const void* map_ = nullptr;
    std::size_t length_ = 0, offset_ = 0;
};
} // namespace metatex::io
