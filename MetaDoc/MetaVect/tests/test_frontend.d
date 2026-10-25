module tests.test_frontend;

import MetaVect : versionString, versionMajor, versionMinor, versionPatch, Status;
import driver.mvect : compile, CompileOptions;
import frontend.AST;
import frontend.Input : SourceText;
import frontend.Lexer : lex;
import frontend.Parser : parse, maxSyntaxDepth;
import frontend.Semacheck_Visitor : checkSemantically;
import frontend.SyntaxError : DiagnosticKind, formatDiagnostic;
import frontend.Token : TokenKind, keywordKind, tokenName;
import std.algorithm : canFind, map;
import std.array : array, replicate;
import std.conv : to;
import std.exception : enforce;
import std.file : dirEntries, SpanMode, readText;
import std.path : buildPath, extension;
import std.stdio : stderr, writeln;
import std.string : indexOf;

// Do not use assert: the contract tests must remain active in release builds.
private void check(bool condition, string message = "check failed", size_t line = __LINE__) {
    enforce(condition, "test_frontend.d:" ~ line.to!string ~ ": " ~ message);
}

private Module parsed(string text, string name = "test.mvct") {
    auto result = parse(new SourceText(text, name));
    check(result.success, result.diagnostics.length ? formatDiagnostic(result.diagnostics[0]) : "no AST");
    return result.ast;
}

private Expression expression(string text) {
    return parsed("result = " ~ text).bindings[0].value;
}

private T node(T)(Expression expression) {
    auto result = cast(T) expression;
    check(result !is null, "unexpected AST node kind: expected " ~ T.stringof);
    return result;
}

private void testSource() {
    auto source = new SourceText("a\r\nb\rc\n\té", "lines.mvct");
    check(source.locationAt(0).line == 1 && source.locationAt(0).column == 1);
    check(source.locationAt(2).line == 1 && source.locationAt(2).column == 3);
    check(source.locationAt(3).line == 2 && source.locationAt(3).column == 1);
    check(source.locationAt(5).line == 3 && source.locationAt(5).column == 1);
    check(source.locationAt(7).line == 4 && source.locationAt(7).column == 1);
    check(source.locationAt(source.text.length).column == 4, "columns must count bytes");
    auto span = source.span(3, 6);
    check(span.start.file == "lines.mvct" && span.start.offset == 3 && span.end.offset == 6);
    check((new SourceText("")).locationAt(0).line == 1);
    check((new SourceText("\n")).locationAt(1).line == 2);
    bool rejected;
    try { source.locationAt(source.text.length + 1); } catch (Exception) { rejected = true; }
    check(rejected, "out-of-bounds offsets must be rejected");
    rejected = false;
    try { source.span(2, 1); } catch (Exception) { rejected = true; }
    check(rejected, "reversed spans must be rejected");
    rejected = false;
    try { parse(null); } catch (Exception) { rejected = true; }
    check(rejected, "a null source must be rejected");
}

private void testTokens() {
    // The D, C, and C++ variants describe one contract; keep them in agreement.
    check(versionString() == "0.1.0" && cast(int) Status.notImplemented == 3);
    check(versionMajor == 0 && versionMinor == 1 && versionPatch == 0);
    check([Status.ok, Status.invalidArgument, Status.outOfMemory, Status.notImplemented]
              .map!(status => cast(int) status).array == [0, 1, 2, 3]);
    // The token contract is total and injective; a renamed or added kind must be
    // reflected here because tokenName uses final switch.
    string[] names;
    foreach (kind; cast(int) 0 .. TokenKind.max + 1) {
        auto name = tokenName(cast(TokenKind) kind);
        check(name.length > 0 && !names.canFind(name), "duplicate token name: " ~ name);
        names ~= name;
        const reserved = cast(TokenKind) kind >= TokenKind.kwLet &&
                         cast(TokenKind) kind <= TokenKind.kwFalse;
        check(keywordKind(name) == (reserved ? cast(TokenKind) kind : TokenKind.identifier));
    }
    check(keywordKind("") == TokenKind.identifier);
    check(keywordKind("Int") == TokenKind.identifier);
    check(keywordKind("letdown") == TokenKind.identifier);
    auto empty = lex(new SourceText(""));
    check(empty.success && empty.tokens.length == 1 && empty.tokens[0].kind == TokenKind.eof);
    // Every reserved word, operator, and delimiter must round-trip through the lexer.
    string text;
    TokenKind[] expected;
    foreach (kind; cast(int) TokenKind.kwLet .. cast(int) TokenKind.max + 1) {
        text ~= tokenName(cast(TokenKind) kind) ~ " ";
        expected ~= cast(TokenKind) kind;
    }
    auto result = lex(new SourceText(text));
    check(result.success && result.tokens.length == expected.length + 1);
    foreach (i, kind; expected) {
        auto token = result.tokens[i];
        check(token.kind == kind && token.lexeme == tokenName(kind));
        check(text[token.span.start.offset .. token.span.end.offset] == token.lexeme);
    }
    result = lex(new SourceText("letdown _x x2 Int trueish"));
    check(result.success && result.tokens.length == 6);
    foreach (token; result.tokens[0 .. $ - 1]) check(token.kind == TokenKind.identifier);
    result = lex(new SourceText("1 1.25 2e3 4.5E-2 01 1. -0.0"));
    check(result.success && result.tokens.length == 10);
    check(result.tokens[0].kind == TokenKind.integer);
    check(result.tokens[1].kind == TokenKind.floating && result.tokens[1].lexeme == "1.25");
    check(result.tokens[3].lexeme == "4.5E-2");
    check(result.tokens[6].kind == TokenKind.dot);
    check(result.tokens[7].kind == TokenKind.minus && result.tokens[8].lexeme == "0.0");
    result = lex(new SourceText("1-- ignored\r\n{- outer {- inner -} end -}\t2"));
    check(result.success && result.tokens.length == 3);
    check(result.tokens[1].lexeme == "2" && result.tokens[1].span.start.line == 2);
    result = lex(new SourceText("{-".replicate(2000) ~ "-}".replicate(2000) ~ "42"));
    check(result.success && result.tokens[0].lexeme == "42", "comments must not use recursive descent");
    result = lex(new SourceText(`"héllo\n\r\t\"\\"`));
    check(result.success && result.tokens[0].kind == TokenKind.stringLiteral);
    check(result.tokens[0].lexeme == `"héllo\n\r\t\"\\"`);
}

private void testLexicalFailures() {
    struct Case { string text; DiagnosticKind kind; string message; }
    Case[] cases = [
        Case("@", DiagnosticKind.invalidCharacter, "unexpected character"),
        Case("é", DiagnosticKind.invalidCharacter, "unexpected character"),
        Case("\0", DiagnosticKind.invalidCharacter, "unexpected character"),
        Case("1e", DiagnosticKind.invalidNumber, "exponent"),
        Case("1e+", DiagnosticKind.invalidNumber, "exponent"),
        Case("1.0E-", DiagnosticKind.invalidNumber, "exponent"),
        Case("12abc", DiagnosticKind.invalidNumber, "suffix"),
        Case("0xff", DiagnosticKind.invalidNumber, "suffix"),
        Case(`"unfinished`, DiagnosticKind.invalidString, "unterminated"),
        Case("\"unfinished\\", DiagnosticKind.invalidString, "unterminated"),
        Case("\"line\nnext\"", DiagnosticKind.invalidString, "newline"),
        Case("\"line\rnext\"", DiagnosticKind.invalidString, "newline"),
        Case(`"\q"`, DiagnosticKind.invalidString, "escape"),
        Case("\"\x01\"", DiagnosticKind.invalidString, "control character"),
        Case("{- outer {- inner -}", DiagnosticKind.unterminatedComment, "unterminated"),
        Case("--\xff", DiagnosticKind.invalidEncoding, "UTF-8"),
        Case("\"\xc0\xaf\"", DiagnosticKind.invalidEncoding, "UTF-8")
    ];
    foreach (test; cases) {
        auto source = new SourceText(test.text, "bad.mvct");
        auto result = lex(source);
        check(!result.success && result.diagnostics.length == 1, "must fail: " ~ test.text);
        auto diagnostic = result.diagnostics[0];
        check(diagnostic.kind == test.kind && diagnostic.message.indexOf(test.message) >= 0);
        check(diagnostic.span.start.offset < diagnostic.span.end.offset);
        check(diagnostic.span.end.offset <= test.text.length);
        check(result.tokens[$ - 1].kind == TokenKind.eof);
        auto syntax = parse(source);
        check(!syntax.success && syntax.ast is null && syntax.diagnostics[0].kind == test.kind);
    }
    auto error = lex(new SourceText("\n  @", "bad.mvct")).diagnostics[0];
    check(formatDiagnostic(error) == "bad.mvct:2:3: error: unexpected character");
    auto unicode = lex(new SourceText("é")).diagnostics[0];
    check(unicode.span.end.offset == 2);
}

private void testDeclarations() {
    check(parsed("").bindings.length == 0);
    check(parsed("-- only a comment").name == "");
    auto ast = parsed("module a.b; import not.a.real.file; import other\n" ~
        "export answer : Int = 42; let message : String = \"hello\" plain = true");
    check(ast.name == "a.b" && ast.imports.length == 2 && ast.imports[0].name == "not.a.real.file");
    check(ast.bindings.length == 3 && ast.bindings[0].exported && !ast.bindings[1].exported);
    // Declarations may omit both "export" and "let", and may follow each other
    // without a separator; newlines are not statement terminators.
    check(ast.bindings[2].name == "plain" && !ast.bindings[2].exported);
    check(ast.bindings[0].annotation.name == "Int" && ast.bindings[1].name == "message");
    check(ast.bindings[2].annotation is null);
    check(node!LiteralExpression(ast.bindings[0].value).spelling == "42");
    auto binding = parsed("x: Int -> Float -> Bool = f").bindings[0];
    check(binding.annotation.domain.name == "Int");
    check(binding.annotation.range.domain.name == "Float" && binding.annotation.range.range.name == "Bool");
    binding = parsed("x: (Int -> Float) -> Bool = f").bindings[0];
    check(binding.annotation.domain.domain.name == "Int" && binding.annotation.range.name == "Bool");
    check(binding.span.start.offset == 0 && binding.span.end.offset == 29);
}

private void testExpressions() {
    auto subtraction = node!BinaryExpression(expression("1 + 2 * 3 - 4 / 2"));
    check(subtraction.operation == TokenKind.minus);
    auto sum = node!BinaryExpression(subtraction.left);
    check(sum.operation == TokenKind.plus);
    check(node!BinaryExpression(sum.right).operation == TokenKind.star);
    check(node!BinaryExpression(subtraction.right).operation == TokenKind.slash);
    subtraction = node!BinaryExpression(expression("9 - 3 - 1"));
    check(node!BinaryExpression(subtraction.left).operation == TokenKind.minus, "operators are left associative");
    auto logical = node!BinaryExpression(expression("a || b && c == d < e + f * g"));
    check(logical.operation == TokenKind.orOr);
    logical = node!BinaryExpression(logical.right);
    check(logical.operation == TokenKind.andAnd);
    logical = node!BinaryExpression(logical.right);
    check(logical.operation == TokenKind.equalEqual);
    logical = node!BinaryExpression(logical.right);
    check(logical.operation == TokenKind.less);
    logical = node!BinaryExpression(logical.right);
    check(logical.operation == TokenKind.plus && node!BinaryExpression(logical.right).operation == TokenKind.star);
    auto unary = node!UnaryExpression(expression("-f(1)"));
    check(unary.operation == TokenKind.minus && node!CallExpression(unary.operand).arguments.length == 1);
    check(node!UnaryExpression(expression("!+true")).operation == TokenKind.bang);
    auto call = node!CallExpression(expression("f()(1, g(2))"));
    check(call.arguments.length == 2 && node!CallExpression(call.callee).arguments.length == 0);
    check(node!CallExpression(call.arguments[1]).arguments.length == 1);
    auto group = node!GroupExpression(expression("(1 + 2)"));
    check(node!BinaryExpression(group.expression).operation == TokenKind.plus);
    check(node!LiteralExpression(expression("()")).kind == LiteralKind.unit);
    check(node!LiteralExpression(expression("false")).kind == LiteralKind.boolean);
    check(node!LiteralExpression(expression(`"\n"`)).spelling == `"\n"`);
    // Values are not converted to host numbers, folded, or canonicalized.
    check(node!LiteralExpression(expression("999999999999999999999999999999999999999999")).kind == LiteralKind.integer);
    check(node!LiteralExpression(expression("1e99999")).spelling == "1e99999");
    check(node!LiteralExpression(node!UnaryExpression(expression("-0.0")).operand).spelling == "0.0");
    auto lambda = node!LambdaExpression(expression("fn(x: Int, y) => x + y"));
    check(lambda.parameters.length == 2 && lambda.parameters[0].annotation.name == "Int");
    check(lambda.parameters[1].annotation is null && node!BinaryExpression(lambda.body).operation == TokenKind.plus);
    check(node!LambdaExpression(expression("fun() => ()")).parameters.length == 0);
    auto local = node!LetExpression(expression("let x: Int = 1 in let y = 2 in x + y"));
    check(local.name == "x" && local.annotation.name == "Int");
    check(node!LetExpression(local.body).name == "y");
    auto conditional = node!IfExpression(expression("if true then if false then 1 else 2 else 3"));
    check(node!IfExpression(conditional.consequent).alternative !is null);
    check(node!LiteralExpression(conditional.alternative).spelling == "3");
    check(node!CallExpression(expression(`ffi("must not execute")`)).arguments.length == 1);
}

private void testSyntaxFailures() {
    string[] cases = [
        "module", "module a.", "import", "module a module b", "x = 1 import b",
        "let = 1", "export = 1", "let x", "let x =", "x = 1 +", "x = (1",
        "x = [1]", "x = {1}", "x = 1.", "x = f(1,)", "x = f(,1)",
        "x = fn(x,) => x", "x = fn(x) x", "x = fn(x: ) => x", "x = fn x => x",
        "x = let y = 1", "x = let y = 1 in", "x = if true 1 else 2",
        "x = if true then 1", "x = if true then 1 else", "x: Int -> = 1",
        "x: forall a. a = f", "type X = Int", "x = match y with z", "x = 1; ;",
        "x = 1 @", "x = 1 }", "x = 1 2"
    ];
    foreach (text; cases) {
        auto result = parse(new SourceText(text));
        check(!result.success && result.ast is null && result.diagnostics.length == 1, "must reject: " ~ text);
    }
    auto missing = parse(new SourceText("x =\n", "missing.mvct"));
    auto error = missing.diagnostics[0];
    check(error.kind == DiagnosticKind.unexpectedToken);
    check(error.span.start.offset == 4 && error.span.end.offset == 4);
    check(formatDiagnostic(error) == "missing.mvct:2:1: error: expected expression, found end of file");
    // A bare binding needs a name; the grammar allows omitting export and let.
    error = parse(new SourceText("x = 1 2")).diagnostics[0];
    check(error.message == "expected identifier, found integer");
    error = parse(new SourceText("x = 1; ;")).diagnostics[0];
    check(error.message == "expected identifier, found ;");
    auto deep = "(".replicate(maxSyntaxDepth + 10) ~ "0" ~ ")".replicate(maxSyntaxDepth + 10);
    auto result = parse(new SourceText("x = " ~ deep));
    check(!result.success && result.diagnostics[0].kind == DiagnosticKind.nestingLimit);
    result = parse(new SourceText("x: " ~ "T -> ".replicate(maxSyntaxDepth + 10) ~ "T = f"));
    check(!result.success && result.diagnostics[0].kind == DiagnosticKind.nestingLimit);
    result = parse(new SourceText("x = " ~ "- ".replicate(maxSyntaxDepth + 10) ~ "1"));
    check(!result.success && result.diagnostics[0].kind == DiagnosticKind.nestingLimit);
    result = parse(new SourceText("x = " ~ "fn() => ".replicate(maxSyntaxDepth + 10) ~ "1"));
    check(!result.success && result.diagnostics[0].kind == DiagnosticKind.nestingLimit);
    check(node!GroupExpression(expression("(".replicate(32) ~ "0" ~ ")".replicate(32))) !is null);
    // Left-associative chains are iterative, not bounded by nesting depth.
    check(node!BinaryExpression(expression("1 + ".replicate(1000) ~ "1")) !is null);
    check(node!CallExpression(expression("f" ~ "()".replicate(1000))) !is null);
}

private void testSemantics() {
    auto module_ = parsed("x = 1\ny = x\nz = fn(value : Int) => value + x");
    auto result = checkSemantically(module_);
    check(result.success && result.ran && result.errors.length == 0);

    auto unresolved = parsed("y = x");
    result = checkSemantically(unresolved);
    check(!result.success && result.errors.length == 1);
    check(result.errors[0].kind == DiagnosticKind.undefinedName);
    check(formatDiagnostic(result.errors[0]).indexOf("undefined name 'x'") >= 0);

    auto duplicate = parsed("x = 1\nx = 2");
    result = checkSemantically(duplicate);
    check(!result.success && result.errors.length == 1);
    check(result.errors[0].kind == DiagnosticKind.duplicateDefinition);

    auto parameters = parsed("f = fn(value : Int, value : Int) => value");
    result = checkSemantically(parameters);
    check(!result.success && result.errors[0].kind == DiagnosticKind.duplicateDefinition);

    auto locals = parsed("f = let value = 1 in let value = 2 in value");
    result = checkSemantically(locals);
    writeln("locals sem=", result.success, " n=", result.errors.length);
    // Inner local bindings shadow outer ones, as the symbol-table contract requires.
    check(result.success && result.errors.length == 0);

}

private void testDeterminism() {
    // Fixed seed and token vocabulary: reproducible malformed-input coverage.
    immutable vocabulary = ["x", "=", "(", ")", "let", "in", "if", "then", "else",
                            "fn", "=>", ",", ":", "Int", "1", "+", ";", "{", `"a"`];
    uint state = 0x4d565443;
    foreach (sample; 0 .. 512) {
        string text;
        foreach (part; 0 .. 32) {
            state = state * 1664525u + 1013904223u;
            text ~= vocabulary[state % vocabulary.length] ~ " ";
        }
        auto source = new SourceText(text);
        auto tokens = lex(source);
        check(tokens.success);
        size_t end;
        foreach (token; tokens.tokens) {
            check(token.span.start.offset >= end && token.span.end.offset >= token.span.start.offset);
            check(text[token.span.start.offset .. token.span.end.offset] == token.lexeme);
            end = token.span.end.offset;
        }
        check(end == text.length);
        auto first = parse(source);
        auto second = parse(source);
        check(first.success == second.success);
        if (!first.success) {
            check(first.ast is null && first.diagnostics.length == 1);
            check(first.diagnostics[0].kind == second.diagnostics[0].kind);
            check(formatDiagnostic(first.diagnostics[0]) == formatDiagnostic(second.diagnostics[0]));
        }
    }
}

private void testRepository(string root) {
    size_t count;
    foreach (entry; dirEntries(buildPath(root, "stdlib"), SpanMode.depth)) {
        if (!entry.isFile || extension(entry.name) != ".mvct") continue;
        auto ast = parsed(readText(entry.name), entry.name);
        check(ast.name.length > 0 && ast.bindings.length == 2);
        ++count;
    }
    check(count > 0, "must exercise existing standard-library interfaces");
    CompileOptions options;
    check(!compile(options).success);
    options.inputPath = buildPath(root, "examples", "first.mvct");
    options.checkSyntaxOnly = true;
    auto result = compile(options);
    check(result.success && result.outputPath == "" && result.diagnostics.length == 0);
    options.checkSyntaxOnly = false;
    result = compile(options);
    check(!result.success && result.diagnostics[0].indexOf("not yet implemented") >= 0);
    options.checkSyntaxOnly = true;
    options.inputPath = buildPath(root, "tests", "fixtures", "invalid.mvct");
    result = compile(options);
    check(!result.success && result.diagnostics[0].indexOf(":2:1: error:") >= 0);
    options.inputPath = buildPath(root, "tests", "fixtures", "does-not-exist.mvct");
    result = compile(options);
    check(!result.success && result.diagnostics[0].indexOf("cannot read input:") == 0);
}

int main(string[] args) {
    testSource();
    testTokens();
    testLexicalFailures();
    testDeclarations();
    testExpressions();
    testSyntaxFailures();
    testSemantics();
    testDeterminism();
    check(args.length == 2, "expected repository root");
    testRepository(args[1]);
    writeln("MetaVect frontend contracts passed");
    return 0;
}
