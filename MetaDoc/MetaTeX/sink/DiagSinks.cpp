#include "DiagSinks.hpp"
#include "../utils/Diagnostics.hpp"
namespace metatex::io {
void DiagnosticSink::write(const Diagnostic& diagnostic) {
    sink_.write(util::formatDiagnostic(diagnostic) + '\n');
}
std::size_t DiagnosticSink::write(const Document& document) {
    std::size_t errors = 0;
    for (const auto& d : document.diagnostics) {
        write(d); errors += d.severity == Diagnostic::Severity::Error;
    }
    sink_.flush(); return errors;
}
}
