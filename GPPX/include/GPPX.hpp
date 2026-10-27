#ifndef GPPX_CPP_HEADER
#define GPPX_CPP_HEADER
#include "GPPX.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
namespace gppx {
struct Diagnostic { std::string path; size_t line = 0; std::string message; bool warning = false; };
struct Syntax {
 std::string directive = "#";
 std::string macro_open; /* empty: expand bare identifiers, otherwise explicit e.g. $(NAME) */
 std::string macro_close = ")";
 std::string line_comment = "//";
 std::string block_open = "/*";
 std::string block_close = "*/";
 bool expand_strings = false;
};
class Error : public std::runtime_error { public: using std::runtime_error::runtime_error; };
class Engine {
public:
 Engine(); ~Engine();
 Engine(const Engine&) = delete; Engine& operator=(const Engine&) = delete;
 Engine(Engine&&) noexcept; Engine& operator=(Engine&&) noexcept;
 std::string process(std::string_view input,std::string virtualPath = "<stdin>");
 std::string processFile(const std::filesystem::path& path);
 void define(std::string name,std::string replacement);
 std::string call(const std::string& name,const std::vector<std::string>& arguments);
 void undef(const std::string& name);
 void includePath(std::filesystem::path path);
 void set(std::string option,std::string value);
 void loadPlugin(const std::filesystem::path& path);
 void registerDirective(std::string name,std::function<std::string(std::string_view)> handler);
 void registerMacro(std::string name,std::function<std::string(const std::vector<std::string>&)> handler);
 void registerRewriter(std::function<std::string(std::string_view)> handler);
 void registerReader(std::string extension,std::function<std::string(const std::filesystem::path&)> handler);
 void registerSink(std::string name,std::function<void(std::string_view)> handler);
 void registerEmbed(std::string name,std::function<std::string(std::string_view)> handler);
 void registerSyntax(std::string name,std::function<void(std::string_view)> handler);
 void registerPMatch(std::string name,std::function<bool(std::string_view,std::string_view)> handler);
 std::vector<Diagnostic> diagnostics() const;
 void diagnostic(std::string message);
 std::string lastError() const;
 bool match(const std::string& engine,std::string_view pattern,std::string_view subject);
 void writeSink(const std::string& name,std::string_view content);
 std::string read(const std::filesystem::path& path);
 /* GPPX plugin ABI access. State is owned by Engine; do not free it. */
 GPPX_State* state() noexcept;
private:
 struct Impl; std::unique_ptr<Impl> impl_;
 friend struct EngineAccess;
};
}
#endif
