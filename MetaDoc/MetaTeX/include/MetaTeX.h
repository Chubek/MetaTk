#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include "MetaVect-Scene.hpp"
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace metatex {

enum class TokenKind { Character, Control, BeginGroup, EndGroup, MathShift,
    Alignment, Parameter, Superscript, Subscript, Space, EndLine, Sentinel };
struct Token {
    TokenKind kind = TokenKind::Character;
    std::string value;
    std::size_t offset = 0, line = 1, column = 1;
};
class Lexer {
public:
    static std::vector<Token> scan(std::string_view source);
};

enum class SpanKind { Text, Bold, Italic, Code, Math, Link, Image, SmallCaps, Underline };
struct Span {
    SpanKind kind = SpanKind::Text;
    std::string text, extra;
};
enum class BlockKind { Paragraph, Heading, ListItem, Quote, Code, DisplayMath,
    HorizontalRule, Note, TableOfContents, PageBreak, VectorGraphic };
struct Block {
    BlockKind kind = BlockKind::Paragraph;
    std::vector<Span> spans;
    int level = 0;             // heading level or list indentation
    bool ordered = false;
    std::string id;
    std::shared_ptr<const metavect::Scene> graphic;
    double graphicWidth = 0;   // 0 = natural width; points, aspect ratio preserved
};
struct Diagnostic {
    enum class Severity { Warning, Error } severity = Severity::Warning;
    std::string message, file;
    std::size_t line = 1, column = 1;
};
struct Document {
    std::string title, author, date;
    std::vector<Block> blocks;
    std::unordered_map<std::string, std::string> references;
    std::vector<Diagnostic> diagnostics;
};
struct Macro {
    int parameters = 0;
    std::vector<Token> replacement;
};

class Engine {
public:
    using Handler = std::function<void(Engine&)>;
    Engine();
    // Compile resets document state; externally registered handlers persist.
    Document compile(std::string source, std::filesystem::path filename = "<memory>");
    // Incremental evaluation for the REPL: preserve definitions, return only new blocks.
    Document compileFragment(std::string source, std::filesystem::path filename = "<repl>");
    void registerCommand(std::string name, Handler handler);
    void defineCommand(std::string name, Handler handler);
    void registerEnvironment(std::string name, Handler onBegin, Handler onEnd = {});
    void report(std::string message, bool error = false);
    std::vector<Token> readArgument();
    std::optional<std::vector<Token>> readOptionalArgument();
    std::string readTextArgument();
    std::string readOptionalText(std::string fallback = {});
    std::string readDelimitedText();
    std::string tokensText(const std::vector<Token>& tokens) const;
    void execute(const std::vector<Token>& tokens);
    void insert(std::vector<Token> tokens);
    void addText(std::string text);
    void addSpan(SpanKind kind, std::string text, std::string extra = {});
    void styledArgument(SpanKind kind);
    void heading(int level);
    void paragraph();
    void startBlock(BlockKind kind, int level = 0, bool ordered = false);
    void addGraphic(metavect::Scene scene, std::string caption, double width = 0);
    void setTitle(std::string title);
    void setAuthor(std::string author);
    void setDate(std::string date);
    void makeTitle();
    void beginEnvironment(const std::string& name);
    void endEnvironment(const std::string& name);
    void item();
    void math(bool display, std::string terminator);
    void defineMacro(std::string name, Macro macro, bool global = false);
    bool hasMacro(const std::string& name) const;
    bool hasCommand(const std::string& name) const;
    std::vector<std::string> commandNames() const;
    void setCounter(const std::string& name, int value);
    int counter(const std::string& name) const;
    void stepCounter(const std::string& name);
    void addToCounter(const std::string& name, int delta);
    void label(std::string key);
    void reference(std::string key);
    std::filesystem::path currentFile() const;
    Document& document() { return document_; }
    const Document& document() const { return document_; }
private:
    struct Environment { Handler begin, end; };
    std::unordered_map<std::string, Handler> handlers_;
    std::unordered_map<std::string, Environment> environments_;
    std::vector<std::unordered_map<std::string, Macro>> scopes_;
    std::vector<std::unordered_map<std::string, Handler>> commandScopes_;
    std::deque<Token> stream_;
    Document document_;
    std::filesystem::path file_;
    std::vector<std::string> envStack_;
    std::unordered_map<std::string,int> counters_;
    std::vector<SpanKind> styles_;
    std::vector<std::vector<SpanKind>> groupStyles_;
    Block active_;
    bool activeValid_ = false;
    bool pendingSpace_ = false;
    int successiveNewlines_ = 0;
    std::string lastHeadingNumber_, lastHeadingId_;
    std::size_t headingSequence_ = 0;
    std::size_t operations_ = 0, includeDepth_ = 0, executionDepth_ = 0;
    bool stopped_ = false, endInput_ = false;
    Token current_;
    Token pop(bool skipSpace = false);
    Token peek(bool skipSpace = false);
    bool consume(TokenKind kind);
    void run(bool nested);
    void command(const std::string& name);
    const Macro* lookup(const std::string& name) const;
    void expand(const Macro& macro);
    void installPrimitives();
    void installScripting();
    Document compileImpl(std::string source, std::filesystem::path filename, bool reset);
    void flush();
    void beginGroup();
    void endGroup();
    void readInput();
    void scanDefinition(bool global);
    void conditional(bool condition);
    void processMathShift();
    void addSpace();
};

// Backends are intentionally independent from the engine and can be replaced.
std::string renderHTML(const Document& doc);
std::string renderPDF(const Document& doc);
// Optional, higher-fidelity backend using Cairo + Pango + Fontconfig + FreeType + HarfBuzz.
// Returns false if those development packages were not available at build time.
bool renderCairoPDF(const Document& doc, const std::filesystem::path& output);
std::string renderText(const Document& doc);
std::string renderJSON(const Document& doc);
std::string renderMathML(std::string_view tex);
void writeFile(const std::filesystem::path& path, std::string_view bytes);
std::string readFile(const std::filesystem::path& path);
std::string readFile(const std::filesystem::path& path, std::size_t maxBytes);

} // namespace metatex
