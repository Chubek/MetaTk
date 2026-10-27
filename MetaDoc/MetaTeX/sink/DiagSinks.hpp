#pragma once
#include "MetaTeX.h"
#include "MetaTeX-Sink.h"
namespace metatex::io {
class DiagnosticSink {
public:
    explicit DiagnosticSink(Sink& sink) : sink_(sink) {}
    void write(const Diagnostic& diagnostic);
    std::size_t write(const Document& document); // Number of errors emitted.
private:
    Sink& sink_;
};
}
