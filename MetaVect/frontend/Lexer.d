/// Deterministic UTF-8 lexer. Identifiers are ASCII; spans and columns count bytes.
module frontend.Lexer;

public import frontend.Input : SourceText, SourceLocation, SourceSpan;
public import frontend.SyntaxError : Diagnostic, DiagnosticKind;
public import frontend.Token : Token, TokenKind, keywordKind;
import std.exception : enforce;
import std.utf : decode, UTFException;

/// Spelled operators, longest first so the scanner always takes the longest match.
private struct OperatorSpelling {
    string spelling;
    TokenKind kind;
}

private immutable OperatorSpelling[] operators = [
    OperatorSpelling("->", TokenKind.arrow),
    OperatorSpelling("=>", TokenKind.fatArrow),
    OperatorSpelling("::", TokenKind.doubleColon),
    OperatorSpelling("==", TokenKind.equalEqual),
    OperatorSpelling("!=", TokenKind.bangEqual),
    OperatorSpelling("<=", TokenKind.lessEqual),
    OperatorSpelling(">=", TokenKind.greaterEqual),
    OperatorSpelling("&&", TokenKind.andAnd),
    OperatorSpelling("||", TokenKind.orOr),
    OperatorSpelling("(", TokenKind.leftParen),
    OperatorSpelling(")", TokenKind.rightParen),
    OperatorSpelling("[", TokenKind.leftBracket),
    OperatorSpelling("]", TokenKind.rightBracket),
    OperatorSpelling("{", TokenKind.leftBrace),
    OperatorSpelling("}", TokenKind.rightBrace),
    OperatorSpelling(",", TokenKind.comma),
    OperatorSpelling(".", TokenKind.dot),
    OperatorSpelling(":", TokenKind.colon),
    OperatorSpelling(";", TokenKind.semicolon),
    OperatorSpelling("+", TokenKind.plus),
    OperatorSpelling("-", TokenKind.minus),
    OperatorSpelling("*", TokenKind.star),
    OperatorSpelling("/", TokenKind.slash),
    OperatorSpelling("=", TokenKind.equal),
    OperatorSpelling("!", TokenKind.bang),
    OperatorSpelling("<", TokenKind.less),
    OperatorSpelling(">", TokenKind.greater),
    OperatorSpelling("|", TokenKind.pipe),
    OperatorSpelling("&", TokenKind.ampersand)
];

struct LexResult {
    Token[] tokens;
    Diagnostic[] diagnostics;
    @property bool success() const { return diagnostics.length == 0; }
}

LexResult lex(SourceText source) {
    enforce(source !is null, "lex requires a source");
    return (new Lexer(source)).run();
}

private bool digit(char c) { return c >= '0' && c <= '9'; }
private bool identifierStart(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
private bool identifierPart(char c) { return identifierStart(c) || digit(c); }

private final class Lexer {
    SourceText source;
    size_t cursor;
    LexResult result;

    this(SourceText source) { this.source = source; }

    void error(DiagnosticKind kind, string message, size_t begin, size_t end) {
        result.diagnostics ~= Diagnostic(message, source.span(begin, end), kind);
    }

    void emit(TokenKind kind, size_t begin) {
        result.tokens ~= Token(kind, source.text[begin .. cursor],
                               source.span(begin, cursor));
    }

    bool starts(string spelling) {
        return source.text.length - cursor >= spelling.length &&
               source.text[cursor .. cursor + spelling.length] == spelling;
    }

    LexResult run() {
        // Validate before scanning so malformed bytes cannot hide in comments/strings.
        size_t index;
        while (index < source.text.length) {
            const begin = index;
            try { decode(source.text, index); }
            catch (UTFException) {
                error(DiagnosticKind.invalidEncoding, "invalid UTF-8 encoding", begin, begin + 1);
                break;
            }
        }
        while (cursor < source.text.length && result.success) scan();
        result.tokens ~= Token(TokenKind.eof, "",
                               source.span(source.text.length, source.text.length));
        return result;
    }

    void scan() {
        const begin = cursor;
        const c = source.text[cursor];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            ++cursor;
            return;
        }
        if (starts("--")) {
            while (cursor < source.text.length && source.text[cursor] != '\n' &&
                   source.text[cursor] != '\r') ++cursor;
            return;
        }
        if (starts("{-")) {
            blockComment();
            return;
        }
        if (identifierStart(c)) {
            while (cursor < source.text.length && identifierPart(source.text[cursor])) ++cursor;
            emit(keywordKind(source.text[begin .. cursor]), begin);
            return;
        }
        if (digit(c)) { number(); return; }
        if (c == '"') { stringLiteral(); return; }
        foreach (operator; operators) {
            if (starts(operator.spelling)) {
                // '-' and '-}' have already been consumed as comment starts above.
                cursor += operator.spelling.length;
                emit(operator.kind, begin);
                return;
            }
        }
        // Use the full UTF-8 code point as the offending range.
        decode(source.text, cursor);
        error(DiagnosticKind.invalidCharacter, "unexpected character", begin, cursor);
    }

    void blockComment() {
        const begin = cursor;
        size_t depth = 1;
        cursor += 2;
        while (cursor < source.text.length) {
            if (starts("{-")) { ++depth; cursor += 2; }
            else if (starts("-}")) {
                cursor += 2;
                if (--depth == 0) return;
            } else ++cursor;
        }
        error(DiagnosticKind.unterminatedComment, "unterminated block comment", begin, cursor);
    }

    void number() {
        const begin = cursor;
        auto kind = TokenKind.integer;
        while (cursor < source.text.length && digit(source.text[cursor])) ++cursor;
        if (cursor + 1 < source.text.length && source.text[cursor] == '.' &&
            digit(source.text[cursor + 1])) {
            kind = TokenKind.floating;
            ++cursor;
            while (cursor < source.text.length && digit(source.text[cursor])) ++cursor;
        }
        if (cursor < source.text.length && (source.text[cursor] == 'e' || source.text[cursor] == 'E')) {
            kind = TokenKind.floating;
            ++cursor;
            if (cursor < source.text.length && (source.text[cursor] == '+' || source.text[cursor] == '-'))
                ++cursor;
            const exponent = cursor;
            while (cursor < source.text.length && digit(source.text[cursor])) ++cursor;
            if (exponent == cursor) {
                error(DiagnosticKind.invalidNumber, "expected digits in exponent", begin, cursor);
                return;
            }
        }
        if (cursor < source.text.length && identifierStart(source.text[cursor])) {
            while (cursor < source.text.length && identifierPart(source.text[cursor])) ++cursor;
            error(DiagnosticKind.invalidNumber, "invalid numeric suffix", begin, cursor);
            return;
        }
        emit(kind, begin);
    }

    void stringLiteral() {
        const begin = cursor++;
        while (cursor < source.text.length) {
            const c = source.text[cursor++];
            if (c == '"') { emit(TokenKind.stringLiteral, begin); return; }
            if (c == '\r' || c == '\n') {
                error(DiagnosticKind.invalidString, "newline in string literal", begin, cursor);
                return;
            }
            if (c == '\\') {
                if (cursor == source.text.length) break;
                const escaped = source.text[cursor++];
                if (escaped != 'n' && escaped != 'r' && escaped != 't' &&
                    escaped != '\\' && escaped != '"') {
                    error(DiagnosticKind.invalidString, "invalid string escape", cursor - 2, cursor);
                    return;
                }
            } else if (cast(ubyte) c < 0x20 || c == 0x7f) {
                error(DiagnosticKind.invalidString, "control character in string literal", cursor - 1, cursor);
                return;
            }
        }
        error(DiagnosticKind.invalidString, "unterminated string literal", begin, cursor);
    }
}

string moduleName() @safe pure nothrow { return "frontend.Lexer"; }
