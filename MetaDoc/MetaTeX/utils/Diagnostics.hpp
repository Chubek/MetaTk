#pragma once
#include "MetaTeX.h"
#include "Format.hpp"
#include <algorithm>
namespace metatex::util {
inline bool hasErrors(const Document& document) {
    return std::any_of(document.diagnostics.begin(), document.diagnostics.end(),
        [](const Diagnostic& d) { return d.severity == Diagnostic::Severity::Error; });
}
inline std::string formatDiagnostic(const Diagnostic& d) {
    return diagnosticText(d.file) + ":" + std::to_string(d.line) + ":" + std::to_string(d.column) + ": " +
        (d.severity == Diagnostic::Severity::Error ? "error: " : "warning: ") + diagnosticText(d.message);
}
}
