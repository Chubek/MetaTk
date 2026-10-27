#pragma once
#include "MetaTeX-Reader.h"
namespace metatex::io {
// Snapshot of a POSIX shared-memory blob. Publisher must finish before this
// constructor and must not resize/change the object during the copy.
class SharedMemoryReader final : public Reader {
public:
    explicit SharedMemoryReader(const std::string& name, std::size_t maxBytes = 16u * 1024u * 1024u);
    std::size_t read(char* destination, std::size_t capacity) override;
private:
    std::string bytes_;
    std::size_t offset_ = 0;
};
}
