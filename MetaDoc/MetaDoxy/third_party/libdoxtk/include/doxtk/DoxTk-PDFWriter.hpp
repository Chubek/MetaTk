#pragma once
#include "doxtk/DoxTk.hpp"

// Optional PDF output integration.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkPDFWriter.cpp.
namespace doxtk {
struct PDFWriterModuleInfo {
    static constexpr std::string_view description = "Optional PDF output integration.";
};
} // namespace doxtk
