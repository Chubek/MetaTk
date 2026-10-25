/// Public token contract for the MetaVect syntax frontend.
///
/// Single source of truth for token kinds and their spellings. A sibling .di is
/// deliberately absent: D emits no code for interface-only modules, and when a
/// module has both files the compiler uses the .d and silently ignores the .di.
/// See frontend/README.md for the layout rule and cmake/check-source.cmake for
/// the guard that enforces it.
module frontend.Token;

public import frontend.Input : SourceLocation, SourceSpan;

enum TokenKind {
    eof, identifier, integer, floating, stringLiteral,
    kwLet, kwIn, kwFun, kwFn, kwType, kwForall,
    kwIf, kwThen, kwElse, kwMatch, kwWith,
    kwModule, kwImport, kwExport, kwTrue, kwFalse,
    leftParen, rightParen, leftBracket, rightBracket, leftBrace, rightBrace,
    comma, dot, colon, semicolon,
    plus, minus, star, slash, equal, bang, less, greater, pipe, ampersand,
    arrow, fatArrow, doubleColon, equalEqual, bangEqual,
    lessEqual, greaterEqual, andAnd, orOr
}

/// Diagnostic spelling of a token kind. Total by construction: `final switch`
/// makes a missing or renamed token kind a compile error.
string tokenName(TokenKind kind) {
    final switch (kind) {
        case TokenKind.eof: return "end of file";
        case TokenKind.identifier: return "identifier";
        case TokenKind.integer: return "integer";
        case TokenKind.floating: return "float";
        case TokenKind.stringLiteral: return "string";
        case TokenKind.kwLet: return "let";
        case TokenKind.kwIn: return "in";
        case TokenKind.kwFun: return "fun";
        case TokenKind.kwFn: return "fn";
        case TokenKind.kwType: return "type";
        case TokenKind.kwForall: return "forall";
        case TokenKind.kwIf: return "if";
        case TokenKind.kwThen: return "then";
        case TokenKind.kwElse: return "else";
        case TokenKind.kwMatch: return "match";
        case TokenKind.kwWith: return "with";
        case TokenKind.kwModule: return "module";
        case TokenKind.kwImport: return "import";
        case TokenKind.kwExport: return "export";
        case TokenKind.kwTrue: return "true";
        case TokenKind.kwFalse: return "false";
        case TokenKind.leftParen: return "(";
        case TokenKind.rightParen: return ")";
        case TokenKind.leftBracket: return "[";
        case TokenKind.rightBracket: return "]";
        case TokenKind.leftBrace: return "{";
        case TokenKind.rightBrace: return "}";
        case TokenKind.comma: return ",";
        case TokenKind.dot: return ".";
        case TokenKind.colon: return ":";
        case TokenKind.semicolon: return ";";
        case TokenKind.plus: return "+";
        case TokenKind.minus: return "-";
        case TokenKind.star: return "*";
        case TokenKind.slash: return "/";
        case TokenKind.equal: return "=";
        case TokenKind.bang: return "!";
        case TokenKind.less: return "<";
        case TokenKind.greater: return ">";
        case TokenKind.pipe: return "|";
        case TokenKind.ampersand: return "&";
        case TokenKind.arrow: return "->";
        case TokenKind.fatArrow: return "=>";
        case TokenKind.doubleColon: return "::";
        case TokenKind.equalEqual: return "==";
        case TokenKind.bangEqual: return "!=";
        case TokenKind.lessEqual: return "<=";
        case TokenKind.greaterEqual: return ">=";
        case TokenKind.andAnd: return "&&";
        case TokenKind.orOr: return "||";
    }
}

/// Reserved word for a spelling, or TokenKind.identifier when it is not reserved.
/// Case sensitive: the language has no folded spelling.
TokenKind keywordKind(string spelling) {
    foreach (kind; cast(int) TokenKind.kwLet .. cast(int) TokenKind.kwFalse + 1) {
        if (spelling == tokenName(cast(TokenKind) kind)) return cast(TokenKind) kind;
    }
    return TokenKind.identifier;
}

struct Token {
    TokenKind kind;
    /// Exact source spelling, never evaluated or normalized.
    string lexeme;
    SourceSpan span;
}