/// Fail-fast recursive descent for declarations/types, Pratt parsing for expressions.
module frontend.Parser;

public import frontend.Input : SourceText, SourceLocation, SourceSpan;
public import frontend.SyntaxError : Diagnostic, DiagnosticKind;
import frontend.AST;
import frontend.Lexer : lex;
import frontend.Token : Token, TokenKind, tokenName;

/// Bounds recursion on hostile input. Iterative operator/call chains do not consume depth.
enum maxSyntaxDepth = 256;

struct ParseResult {
    Module ast;
    Diagnostic[] diagnostics;
    @property bool success() const { return ast !is null && diagnostics.length == 0; }
}

ParseResult parse(SourceText source) {
    auto lexical = lex(source);
    if (!lexical.success) return ParseResult(null, lexical.diagnostics);
    try {
        return ParseResult((new Parser(lexical.tokens)).parseModule(), []);
    } catch (ParseFailure failure) {
        // A partial tree is deliberately not exposed as a usable program.
        return ParseResult(null, [failure.diagnostic]);
    }
}

private final class ParseFailure : Exception {
    Diagnostic diagnostic;
    this(Diagnostic diagnostic) { super(diagnostic.message); this.diagnostic = diagnostic; }
}

private final class Parser {
    Token[] tokens;
    size_t cursor;
    size_t depth;

    this(Token[] tokens) { this.tokens = tokens; }

    Token current() { return tokens[cursor]; }
    Token previous() { return tokens[cursor - 1]; }
    bool at(TokenKind kind) { return current.kind == kind; }

    bool accept(TokenKind kind) {
        if (!at(kind)) return false;
        ++cursor;
        return true;
    }

    void fail(string expected) {
        throw new ParseFailure(Diagnostic("expected " ~ expected ~ ", found " ~
            tokenName(current.kind), current.span, DiagnosticKind.unexpectedToken));
    }

    Token expect(TokenKind kind) {
        auto token = current;
        if (!accept(kind)) fail(tokenName(kind));
        return token;
    }

    void enter() {
        if (depth >= maxSyntaxDepth)
            throw new ParseFailure(Diagnostic("syntax nesting limit exceeded", current.span,
                                               DiagnosticKind.nestingLimit));
        ++depth;
    }

    SourceSpan through(SourceSpan begin, SourceSpan end) {
        return SourceSpan(begin.start, end.end);
    }

    string qualifiedName() {
        auto name = expect(TokenKind.identifier).lexeme;
        while (accept(TokenKind.dot)) name ~= "." ~ expect(TokenKind.identifier).lexeme;
        return name;
    }

    Module parseModule() {
        auto result = new Module(through(current.span, tokens[$ - 1].span));
        if (accept(TokenKind.kwModule)) {
            result.name = qualifiedName();
            accept(TokenKind.semicolon);
        }
        while (accept(TokenKind.kwImport)) {
            const begin = previous.span;
            auto name = qualifiedName();
            result.imports ~= ImportDeclaration(name, through(begin, previous.span));
            accept(TokenKind.semicolon);
        }
        while (!at(TokenKind.eof)) {
            // A binding may start with neither "export" nor "let"; see the
            // grammar in frontend/README.md before tightening this.
            const begin = current.span;
            const exported = accept(TokenKind.kwExport);
            accept(TokenKind.kwLet);
            auto name = expect(TokenKind.identifier);
            auto annotation = optionalAnnotation();
            expect(TokenKind.equal);
            auto value = expression();
            result.bindings ~= new Binding(through(begin, value.span), exported,
                                           name.lexeme, annotation, value);
            accept(TokenKind.semicolon);
        }
        return result;
    }

    TypeExpression optionalAnnotation() {
        return accept(TokenKind.colon) ? typeExpression() : null;
    }

    TypeExpression typeExpression() {
        enter();
        scope(exit) --depth;
        TypeExpression result;
        if (accept(TokenKind.leftParen)) {
            const begin = previous.span;
            result = typeExpression();
            const end = expect(TokenKind.rightParen).span;
            result.span = through(begin, end);
        } else {
            auto name = expect(TokenKind.identifier);
            result = new TypeExpression(name.span, name.lexeme);
        }
        if (accept(TokenKind.arrow)) {
            auto range = typeExpression();
            result = new TypeExpression(through(result.span, range.span), result, range);
        }
        return result;
    }

    int precedence(TokenKind kind) {
        switch (kind) {
        case TokenKind.orOr: return 1;
        case TokenKind.andAnd: return 2;
        case TokenKind.equalEqual: case TokenKind.bangEqual: return 3;
        case TokenKind.less: case TokenKind.lessEqual:
        case TokenKind.greater: case TokenKind.greaterEqual: return 4;
        case TokenKind.plus: case TokenKind.minus: return 5;
        case TokenKind.star: case TokenKind.slash: return 6;
        default: return -1;
        }
    }

    Expression expression(int minimum = 0) {
        enter();
        scope(exit) --depth;
        auto left = prefix();
        while (true) {
            if (at(TokenKind.leftParen)) {
                left = call(left);
                continue;
            }
            const priority = precedence(current.kind);
            if (priority < minimum) break;
            const operation = current.kind;
            ++cursor;
            auto right = expression(priority + 1);
            left = new BinaryExpression(through(left.span, right.span), operation, left, right);
        }
        return left;
    }

    Expression call(Expression callee) {
        expect(TokenKind.leftParen);
        Expression[] arguments;
        if (!at(TokenKind.rightParen)) {
            do { arguments ~= expression(); } while (accept(TokenKind.comma));
        }
        auto end = expect(TokenKind.rightParen);
        return new CallExpression(through(callee.span, end.span), callee, arguments);
    }

    Expression prefix() {
        auto token = current;
        switch (token.kind) {
        case TokenKind.identifier:
            ++cursor;
            return new IdentifierExpression(token.span, token.lexeme);
        case TokenKind.integer: case TokenKind.floating: case TokenKind.stringLiteral:
        case TokenKind.kwTrue: case TokenKind.kwFalse:
            ++cursor;
            LiteralKind kind;
            if (token.kind == TokenKind.integer) kind = LiteralKind.integer;
            else if (token.kind == TokenKind.floating) kind = LiteralKind.floating;
            else if (token.kind == TokenKind.stringLiteral) kind = LiteralKind.stringLiteral;
            else kind = LiteralKind.boolean;
            return new LiteralExpression(token.span, kind, token.lexeme);
        case TokenKind.minus: case TokenKind.bang: case TokenKind.plus:
            ++cursor;
            auto operand = expression(7);
            return new UnaryExpression(through(token.span, operand.span), token.kind, operand);
        case TokenKind.leftParen:
            ++cursor;
            if (accept(TokenKind.rightParen))
                return new LiteralExpression(through(token.span, previous.span), LiteralKind.unit, "()");
            auto inner = expression();
            auto end = expect(TokenKind.rightParen);
            return new GroupExpression(through(token.span, end.span), inner);
        case TokenKind.kwIf: return conditional();
        case TokenKind.kwLet: return letExpression();
        case TokenKind.kwFn: case TokenKind.kwFun: return lambda();
        default:
            fail("expression");
            return null; // fail always throws
        }
    }

    Expression conditional() {
        auto begin = expect(TokenKind.kwIf);
        auto condition = expression();
        expect(TokenKind.kwThen);
        auto consequent = expression();
        expect(TokenKind.kwElse);
        auto alternative = expression();
        return new IfExpression(through(begin.span, alternative.span), condition, consequent, alternative);
    }

    Expression letExpression() {
        auto begin = expect(TokenKind.kwLet);
        auto name = expect(TokenKind.identifier);
        auto annotation = optionalAnnotation();
        expect(TokenKind.equal);
        auto initializer = expression();
        expect(TokenKind.kwIn);
        auto body = expression();
        return new LetExpression(through(begin.span, body.span), name.lexeme, annotation, initializer, body);
    }

    Expression lambda() {
        auto begin = current;
        ++cursor;
        expect(TokenKind.leftParen);
        Parameter[] parameters;
        if (!at(TokenKind.rightParen)) {
            do {
                auto name = expect(TokenKind.identifier);
                auto annotation = optionalAnnotation();
                parameters ~= Parameter(name.lexeme, annotation, through(name.span, previous.span));
            } while (accept(TokenKind.comma));
        }
        expect(TokenKind.rightParen);
        expect(TokenKind.fatArrow);
        auto body = expression();
        return new LambdaExpression(through(begin.span, body.span), parameters, body);
    }
}

string moduleName() @safe pure nothrow { return "frontend.Parser"; }
