#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace parzek {

enum class DiagnosticSeverity {
  Info,
  Warning,
  Error
};

struct Diagnostic {
  DiagnosticSeverity severity{DiagnosticSeverity::Error};
  std::string message;
  std::string file;
  std::size_t line{1};
  std::size_t column{1};
};

struct CompileOptions {
  std::string output_directory{"."};
  std::optional<std::string> output_basename;
  std::optional<std::string> visitor_header;
  std::string source_name{"<memory>"};
};

struct CompileResult {
  bool success{false};
  std::string parser_header_path;
  std::string parser_source_path;
  std::optional<std::string> visitor_header_path;
  std::vector<Diagnostic> diagnostics;
};

CompileResult compile_grammar_string(std::string_view grammar_source,
                                     const CompileOptions& options);

CompileResult compile_grammar_file(const std::string& grammar_path,
                                   const CompileOptions& options);

int run_cli(int argc, char** argv);

}  // namespace parzek
