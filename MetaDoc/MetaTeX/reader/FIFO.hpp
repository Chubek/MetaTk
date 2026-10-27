#pragma once
#include "MetaTeX-Reader.h"
namespace metatex::io {
// Existing FIFO: POSIX open waits for a writer; EOF follows its close.
class FIFOReader final : public DescriptorReader {
public:
    explicit FIFOReader(const std::filesystem::path& path);
};
}
