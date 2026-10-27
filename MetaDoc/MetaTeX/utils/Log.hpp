#pragma once
#include "Format.hpp"
#include <mutex>
#include <ostream>
namespace metatex::util {
enum class LogLevel { Debug, Info, Warning, Error, Off };
// The borrowed stream must outlive the logger. Whole records are serialized.
class Logger {
public:
    explicit Logger(std::ostream& stream, LogLevel threshold = LogLevel::Info)
        : stream_(stream), threshold_(threshold) {}
    void setLevel(LogLevel level) { std::lock_guard lock(mutex_); threshold_ = level; }
    void log(LogLevel level, std::string_view message) {
        std::lock_guard lock(mutex_);
        if (level < threshold_ || level == LogLevel::Off) return;
        const char* name = level == LogLevel::Debug ? "debug" : level == LogLevel::Info ?
            "info" : level == LogLevel::Warning ? "warning" : "error";
        stream_ << name << ": " << diagnosticText(message) << '\n'; stream_.flush();
    }
private:
    std::ostream& stream_;
    LogLevel threshold_;
    std::mutex mutex_;
};
}
