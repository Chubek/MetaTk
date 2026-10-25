/// Syntax only: nodes never resolve names, evaluate expressions, or choose a backend.
module frontend.AST;

public import frontend.Input : SourceLocation, SourceSpan;
import frontend.Token : TokenKind;

abstract class Node {
    SourceSpan span;
    this(SourceSpan span) { this.span = span; }
}

/// A function type has both domain/range; a named type has neither.
final class TypeExpression : Node {
    string name;
    TypeExpression domain;
    TypeExpression range;
    this(SourceSpan span, string name) { super(span); this.name = name; }
    this(SourceSpan span, TypeExpression domain, TypeExpression range) {
        super(span);
        this.domain = domain;
        this.range = range;
    }
}

abstract class Expression : Node {
    this(SourceSpan span) { super(span); }
}

final class IdentifierExpression : Expression {
    string name;
    this(SourceSpan span, string name) { super(span); this.name = name; }
}

enum LiteralKind { integer, floating, stringLiteral, boolean, unit }

final class LiteralExpression : Expression {
    LiteralKind kind;
    /// Original spelling, including string quotes/escapes. No host numeric conversion.
    string spelling;
    this(SourceSpan span, LiteralKind kind, string spelling) {
        super(span);
        this.kind = kind;
        this.spelling = spelling;
    }
}

final class GroupExpression : Expression {
    Expression expression;
    this(SourceSpan span, Expression expression) { super(span); this.expression = expression; }
}

final class UnaryExpression : Expression {
    TokenKind operation;
    Expression operand;
    this(SourceSpan span, TokenKind operation, Expression operand) {
        super(span);
        this.operation = operation;
        this.operand = operand;
    }
}

final class BinaryExpression : Expression {
    TokenKind operation;
    Expression left;
    Expression right;
    this(SourceSpan span, TokenKind operation, Expression left, Expression right) {
        super(span);
        this.operation = operation;
        this.left = left;
        this.right = right;
    }
}

final class CallExpression : Expression {
    Expression callee;
    Expression[] arguments;
    this(SourceSpan span, Expression callee, Expression[] arguments) {
        super(span);
        this.callee = callee;
        this.arguments = arguments;
    }
}

struct Parameter {
    string name;
    TypeExpression annotation; // null when omitted
    SourceSpan span;
}

final class LambdaExpression : Expression {
    Parameter[] parameters;
    Expression body;
    this(SourceSpan span, Parameter[] parameters, Expression body) {
        super(span);
        this.parameters = parameters;
        this.body = body;
    }
}

final class LetExpression : Expression {
    string name;
    TypeExpression annotation;
    Expression initializer;
    Expression body;
    this(SourceSpan span, string name, TypeExpression annotation,
         Expression initializer, Expression body) {
        super(span);
        this.name = name;
        this.annotation = annotation;
        this.initializer = initializer;
        this.body = body;
    }
}

final class IfExpression : Expression {
    Expression condition;
    Expression consequent;
    Expression alternative;
    this(SourceSpan span, Expression condition, Expression consequent, Expression alternative) {
        super(span);
        this.condition = condition;
        this.consequent = consequent;
        this.alternative = alternative;
    }
}

final class Binding : Node {
    bool exported;
    string name;
    TypeExpression annotation;
    Expression value;
    this(SourceSpan span, bool exported, string name, TypeExpression annotation, Expression value) {
        super(span);
        this.exported = exported;
        this.name = name;
        this.annotation = annotation;
        this.value = value;
    }
}

struct ImportDeclaration {
    string name;
    SourceSpan span;
}

final class Module : Node {
    string name; // empty for an anonymous source unit
    ImportDeclaration[] imports;
    Binding[] bindings;
    this(SourceSpan span) { super(span); }
}

string moduleName() @safe pure nothrow { return "frontend.AST"; }
