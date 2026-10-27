#pragma once
#include <cstddef>
#include <array>
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
#include <unordered_set>
#include <vector>
#include <variant>

namespace metatex {

enum class TokenKind { Character, Control, BeginGroup, EndGroup, MathShift,
    Alignment, Parameter, Superscript, Subscript, Space, EndLine, Sentinel };
struct Token {
    TokenKind kind = TokenKind::Character;
    std::string value;
    std::size_t offset = 0, line = 1, column = 1;
    bool expansionProtected = false;
    bool active = false;
    int category = 12;
    bool definitionOpaque = false; // Scanner-only: copy from \the into an expanded definition.
    bool literalParameter = false; // Replacement storage: literal # rather than a parameter reference.
    std::string controlName() const { return active?std::string(1,'\1')+value:value; }
};
class Lexer {
public:
    using Catcodes = std::array<int,256>;
    Lexer() = default;
    explicit Lexer(std::string source);
    static Catcodes defaultCatcodes();
    Token next(const Catcodes& catcodes);
    static std::vector<Token> scan(std::string_view source);
private:
    std::string source_;
    std::size_t position_=0,line_=1,column_=1,emitted_=0;
    enum class State { NewLine, MidLine, SkipSpaces } state_=State::NewLine;
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
    std::vector<Token> prefix;
    std::vector<std::vector<Token>> delimiters;
    std::optional<std::vector<Token>> optionalFirst = std::nullopt;
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
    std::unordered_set<std::string> expandablePrimitives_;
    std::unordered_map<std::string, Environment> environments_;
    std::vector<std::unordered_map<std::string, Macro>> scopes_;
    std::vector<std::unordered_map<std::string, Handler>> commandScopes_;
    enum class RegisterKind { Count, Dimension, Glue, Tokens };
    struct Register { RegisterKind kind; int index; };
    struct Glue {int width=0,stretch=0,shrink=0,stretchOrder=0,shrinkOrder=0;};
    using TokenList = std::shared_ptr<const std::vector<Token>>;
    using RegisterValue = std::variant<int,Glue,TokenList>;
    std::vector<std::unordered_map<int, RegisterValue>> registerScopes_;
    std::vector<std::unordered_map<std::string, Register>> registerAliasScopes_;
    std::unordered_set<std::string> registerPrimitives_;
    bool globalAssignment_ = false;
    Lexer::Catcodes catcodes_=Lexer::defaultCatcodes();
    std::vector<std::unordered_map<int,int>> catcodeSaves_;
    std::vector<std::vector<Token>> afterGroups_;
    std::optional<Lexer> source_;
    std::vector<bool> conditions_;
    std::size_t conditionalFloor_=0;
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
    std::size_t expansionDepth_ = 0;
    bool stopped_ = false, endInput_ = false;
    Token current_;
    Token pop(bool skipSpace = false);
    bool ensureToken();
    void executeSource(std::string source);
    Token peek(bool skipSpace = false);
    bool consume(TokenKind kind);
    void run(bool nested);
    void command(const std::string& name);
    const Macro* lookup(const std::string& name) const;
    void expand(const Macro& macro);
    bool expandToken(const Token& token, bool definition = false);
    Token expanded(bool skipSpace = false, bool definition = false);
    int scanInteger();
    void installExpansion();
    void installRegisters();
    std::optional<Register> registerAlias(const std::string& name) const;
    std::optional<Register> registerToken(const Token& token);
    std::optional<Register> scanRegister();
    int registerValue(Register reg) const;
    int scanRegisterIndex();
    void assignRegister(Register reg);
    void storeRegister(Register reg, RegisterValue value);
    void defineRegister(std::string name, Register reg);
    bool scanKeyword(std::string_view keyword);
    std::optional<int> scanDimension(int* order = nullptr);
    static std::string dimensionText(int value);
    std::optional<Glue> scanGlue();
    Glue glueValue(Register reg) const;
    static std::string glueText(const Glue& glue);
    void arithmeticGlue(Register reg, const std::string& operation);
    TokenList tokenListValue(Register reg) const;
    void assignTokenList(Register reg);
    void scanOptionalEquals();
    void installPrimitives();
    void installScripting();
    Document compileImpl(std::string source, std::filesystem::path filename, bool reset);
    void flush();
    void beginGroup();
    void endGroup();
    void readInput();
    void scanDefinition(bool global, bool expandReplacement = false);
    void conditional(bool condition);
    void finishConditional(bool alternative);
    void assignCatcode();
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
