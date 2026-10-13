/**
 * Composable, single-module DSL construction toolkit for D.
 *
 * Port of DSLtk.hpp using template mixins, variadic tuples, static foreach,
 * string template parameters, operator templates, and CTFE. Import DSLtk;
 * no dependencies beyond Phobos are required.
 *
 * Features are injected directly into a struct or class (no CRTP base):
 * ---
 * struct MathDSL {
 *     mixin DSL!(Pipeline, Operators, AST, ExprTemplates);
 *     double val;
 *     double expr_value() const { return val; }
 * }
 * auto result = 5 | pipe((int x) => x * 2) | pipe((int x) => x + 1);
 * auto tree = node!"add"(leaf!"num"(1), leaf!"num"(2));
 * auto table = match(when!1((int key) => "one"),
 *                    otherwise((int key) => "other"));
 * auto generic_stage = pipe!((x) => x + x)();
 * auto parsed = run_parser(ch('a') & ch('b').many(), "abbb");
 * enum Color { Red = -1000, Green = 0, Blue = 1000 }
 * static assert(enum_name!(Color.Red)() == "Red");
 * auto name = enum_switch!((c) => enum_name!(typeof(c).value)())(Color.Blue);
 * ---
 *
 * All feature-independent factories remain available at module scope.
 * D strings can be template arguments directly; FixedString is optional.
 * Parser repetition uses p.many() / many(p), because unary * is not an
 * overloadable D struct operator. Predicate negation uses ~p or p.negated(),
 * since D's ! operator always produces a bool. Parser and predicate compositions
 * keep concrete callable types, including stateful functors, without type erasure.
 * Generic lambdas are passed as aliases (pipe!fn(), predicate!fn(),
 * when!(key, fn)(), otherwise!fn(), enum_switch!fn(value)); typed functions,
 * delegates and callable structs can also be passed as ordinary arguments.
 * LiteralSet.parse can run at compile time when its handlers support CTFE.
 * Enum reflection uses declared members without C++'s numeric search limits;
 * duplicate-valued members have the first declared name, sorted by value.
 * PEG patterns are greedy prefix matches; pattern!S matches the whole input.
 * PEG rules/definitions/matchers are reference types, preserving rule identity
 * when new rules are appended. ASTNode is a deeply copied value type.
 * Memoization, lazy values and mutable PEG state are not synchronized.
 * Custom feature tags may supply a nested mixin template named Mixin.
 * Native enum formatting and bitwise operators replace C++ stream integrations;
 * EnumArray, EnumBitset and EnumSet provide the enum-indexed containers.
 * SExpressions / EqualitySaturation provide parsed rules, e-matching,
 * congruence rebuilding, bounded saturation and minimum-node extraction.
 * Serialization / AbstractIO are user-backend protocols. Visitors can be
 * derived from runtime ASTs or CTFE schemas. StaticRegex and BinaryStream use
 * byte semantics. Preprocessor supports macros/includes/conditionals and an
 * optional Lua bridge. CFG implements explicit productions and Earley parsing.
 * See manual/ch30-language-facilities.md for examples and supported syntax.
 *
 * Run the embedded tests: ldc2 -unittest -main DSLtk.d -of=dsltk-test
 */
module DSLtk;

import std.conv : to;
import std.meta : AliasSeq, staticIndexOf;
import std.traits : EnumMembers, OriginalType, Unqual, isArray, isIntegral;
import std.typecons : Nullable, Tuple, tuple;

// Feature tags and native feature composition.
struct Pipeline {}
struct Operators {}
struct PatternMatch {}
struct AST {}
struct Rewrite {}
struct ExprTemplates {}
struct CustomLiterals {}
struct Memoization {}
struct LazyFeature {}
struct Monadic {}
struct ResultFeature {}
struct CombinatorParser {}
struct TaskPipeline {}
struct EnumReflection {}
struct SExpressions {}
struct EqualitySaturation {}
struct Serialization {}
struct AbstractIO {}
struct Visitors {}
struct StaticRegularExpressions {}
struct Preprocessing {}
struct ContextFreeGrammar {}

/// Compose features in the enclosing aggregate: mixin DSL!(Pipeline, AST).
mixin template DSL(Features...)
{
    import std.meta : AliasSeq, NoDuplicates, staticIndexOf;
    import DSLtk;
    alias DSLFeatures = AliasSeq!Features;
    static assert(Features.length == NoDuplicates!Features.length,
                  "DSL features must be unique");
    static if (staticIndexOf!(Pipeline, Features) >= 0)
        static auto wrap(T)(T value) { return value; }
    static if (staticIndexOf!(Operators, Features) >= 0)
        static auto make_pred(F)(F fn) { return DSLtk.predicate(fn); }
    static if (staticIndexOf!(AST, Features) >= 0)
    {
        static auto make_leaf(string tag, T)(T value) { return DSLtk.leaf!tag(value); }
        static auto make_node(string tag, C...)(C children) { return DSLtk.node!tag(children); }
    }
    static if (staticIndexOf!(Rewrite, Features) >= 0)
        ASTNode rewrite(ASTNode tree) { return rules.apply(tree); }
    static if (staticIndexOf!(CustomLiterals, Features) >= 0)
        static real parse_literal(string input) { return literals.parse(input); }
    static if (staticIndexOf!(Memoization, Features) >= 0)
        void clear_cache() { memo_cache = null; }
    static if (staticIndexOf!(ExprTemplates, Features) >= 0)
        mixin ExpressionOperators;
    static foreach (Feature; Features)
    {
        static if (__traits(hasMember, Feature, "Mixin")) mixin Feature.Mixin;
    }
}

template HasFeature(T, F)
{
    static if (__traits(hasMember, T, "DSLFeatures"))
        enum HasFeature = staticIndexOf!(F, T.DSLFeatures) >= 0;
    else enum HasFeature = false;
}
enum Pipeable(T) = HasFeature!(T, Pipeline);
enum ExprLike(T) = HasFeature!(T, ExprTemplates) && __traits(hasMember, T, "expr_value");
enum HasLiterals(T) = HasFeature!(T, CustomLiterals) && __traits(hasMember, T, "literals");
enum Rewritable(T) = HasFeature!(T, Rewrite) && __traits(hasMember, T, "rules");

/// Convenience wrapper; native string template arguments are preferred.
struct FixedString(string text)
{
    enum value = text;
    string view() const { return text; }
    alias view this;
}

/// Bind an alias callable (including a generic lambda) without runtime storage.
struct StaticCallable(alias fn)
{
    auto opCall(A...)(auto ref A args) { return fn(args); }
}

struct Predicate(F)
{
    F fn;
    bool opCall(T)(auto ref T value) { return cast(bool) fn(value); }
    auto opBinary(string op, G)(Predicate!G other) if (op == "&" || op == "|")
    {
        return Predicate!(PredicateBinary!(op, F, G))(PredicateBinary!(op, F, G)(fn, other.fn));
    }
    auto opUnary(string op)() if (op == "~")
    {
        return Predicate!(PredicateNot!F)(PredicateNot!F(fn));
    }
    auto negated()() { return ~this; }
}
private struct PredicateBinary(string op, F, G)
{
    F left;
    G right;
    bool opCall(T)(auto ref T value)
    {
        static if (op == "&") return left(value) && right(value);
        else return left(value) || right(value);
    }
}
private struct PredicateNot(F)
{
    F fn;
    bool opCall(T)(auto ref T value) { return !fn(value); }
}
auto predicate(F)(F fn) { return Predicate!F(fn); }
auto predicate(alias fn)() { return predicate(StaticCallable!fn()); }

struct PipeStage(F)
{
    F fn;
    auto opBinaryRight(string op, T)(auto ref T value) if (op == "|") { return fn(value); }
}
auto pipe(F)(F fn) { return PipeStage!F(fn); }
auto pipe(alias fn)() { return pipe(StaticCallable!fn()); }

// Whole-input patterns: literals, '.', classes/ranges, anchors and */+.
struct pattern(string text)
{
    enum value = text;
    static bool matches(string input) { return wholePattern(text, input); }
}
private bool classTest(string cls, char c)
{
    for (size_t i = 0; i < cls.length;)
    {
        if (i + 2 < cls.length && cls[i + 1] == '-')
        {
            if (c >= cls[i] && c <= cls[i + 2]) return true;
            i += 3;
        }
        else if (cls[i++] == c) return true;
    }
    return false;
}
private bool wholePattern(string pat, string input)
{
    if (!pat.length) return !input.length;
    if (pat[0] == '^') return wholePattern(pat[1 .. $], input);
    if (pat == "$") return !input.length;
    char first = pat[0];
    string cls;
    size_t advance = 1;
    if (first == '[')
    {
        while (advance < pat.length && pat[advance] != ']') ++advance;
        if (advance == pat.length) return false;
        cls = pat[1 .. advance++];
    }
    char quant;
    if (advance < pat.length && (pat[advance] == '*' || pat[advance] == '+'))
        quant = pat[advance++];
    bool accepts(char c) { return first == '[' ? classTest(cls, c) : first == '.' || first == c; }
    if (quant == '*' || quant == '+')
    {
        size_t count;
        while (count < input.length && accepts(input[count])) ++count;
        size_t minimum = quant == '+' ? 1 : 0;
        // Backtrack through repetitions so [a-z]+z can match "az".
        for (size_t n = count + 1; n > minimum;)
            if (wholePattern(pat[advance .. $], input[--n .. $])) return true;
        return false;
    }
    return input.length && accepts(input[0]) && wholePattern(pat[advance .. $], input[1 .. $]);
}

struct WhenClause(alias key, F)
{
    F handler;
    enum isOtherwise = false;
    bool accepts(K)(K candidate)
    {
        static if (__traits(compiles, key.matches(candidate))) return key.matches(candidate);
        else static if (__traits(compiles, candidate == key)) return candidate == key;
        else return false;
    }
}
struct OtherwiseClause(F)
{
    F handler;
    enum isOtherwise = true;
}
auto when(alias key, F)(F fn) { return WhenClause!(key, F)(fn); }
auto when(alias key, alias fn)() { return when!key(StaticCallable!fn()); }
auto otherwise(F)(F fn) { return OtherwiseClause!F(fn); }
auto otherwise(alias fn)() { return otherwise(StaticCallable!fn()); }
private template MatchReturns(K, A, C...)
{
    static if (!C.length) alias MatchReturns = AliasSeq!();
    else alias MatchReturns = AliasSeq!(typeof(C[0].init.handler(K.init, A.init.expand)),
                                        MatchReturns!(K, A, C[1 .. $]));
}
struct MatchTable(C...)
{
    Tuple!C clauses;
    auto opCall(K, A...)(K key, A args)
    {
        import std.traits : CommonType;
        alias R = CommonType!(MatchReturns!(K, Tuple!A, C));
        static assert(!is(R == void), "match handlers must have a common non-void result");
        static foreach (i; 0 .. C.length)
        {
            static if (!C[i].isOtherwise)
                if (clauses[i].accepts(key)) return cast(R) clauses[i].handler(key, args);
        }
        static foreach (i; 0 .. C.length)
        {
            static if (C[i].isOtherwise) return cast(R) clauses[i].handler(key, args);
        }
        throw new Exception("DSLtk.match: no clause matched and no otherwise");
    }
}
auto match(C...)(C clauses)
{
    static assert(C.length > 0, "match requires at least one clause");
    return MatchTable!C(tuple(clauses));
}

/// Recursive runtime AST, with deep copies of mutable child storage.
struct ASTNode
{
    private string tag_;
    private string value_;
    private ASTNode[] children_;
    private bool leaf_ = true;
    this(string tag, string value) { tag_ = tag; value_ = value; }
    this(string tag, ASTNode[] children) { tag_ = tag; children_ = children.dup; leaf_ = false; }
    this(this) { children_ = children_.dup; }
    string tag() const { return tag_; }
    bool is_leaf() const { return leaf_; }
    string value() const { return value_; }
    ref ASTNode[] children() return { return children_; }
    const(ASTNode)[] children() const { return children_; }
    ref ASTNode child(size_t i) return { return children_[i]; }
    ref const(ASTNode) child(size_t i) const return { return children_[i]; }
    size_t size() const { return children_.length; }
    string dump() const
    {
        string result = "(" ~ tag_;
        if (leaf_) { if (value_.length) result ~= " " ~ value_; }
        else foreach (ref c; children_) result ~= " " ~ c.dump();
        return result ~ ")";
    }
    string toString() const { return dump(); }
    bool opEquals(ref const ASTNode other) const
    {
        return tag_ == other.tag_ && leaf_ == other.leaf_ &&
            (leaf_ ? value_ == other.value_ : children_ == other.children_);
    }
}
ASTNode leaf(string tag, T)(T value) { return ASTNode(tag, to!string(value)); }
ASTNode node(string tag, C...)(C children)
{
    ASTNode[] result;
    static foreach (i; 0 .. C.length)
    {
        static assert(is(Unqual!(C[i]) == ASTNode), "node children must be ASTNode values");
        result ~= children[i];
    }
    return ASTNode(tag, result);
}

struct RewriteRule(string tag, P, F)
{
    P pred;
    F trans;
    enum name = tag;
    Nullable!ASTNode try_apply(ASTNode n)
    {
        if (pred(n)) return Nullable!ASTNode(trans(n));
        return Nullable!ASTNode.init;
    }
}
auto rule(string tag, P, F)(P pred, F trans) { return RewriteRule!(tag, P, F)(pred, trans); }
struct RewriteSet(R...)
{
    Tuple!R rules;
    size_t max_iterations = 100;
    ASTNode apply(ASTNode root)
    {
        foreach (i; 0 .. max_iterations)
        {
            bool changed;
            root = rewrite_once(root, changed);
            if (!changed) break;
        }
        return root;
    }
    private ASTNode rewrite_once(ASTNode current, ref bool changed)
    {
        if (!current.is_leaf())
            foreach (ref c; current.children()) c = rewrite_once(c, changed);
        static foreach (i; 0 .. R.length)
        {{
            auto replacement = rules[i].try_apply(current);
            if (!replacement.isNull)
            {
                auto next = replacement.get;
                changed |= next != current;
                current = next;
            }
        }}
        return current;
    }
}
auto rewrite_set(R...)(R rules) { return RewriteSet!R(tuple(rules)); }

// Lazy expression trees. Array-valued operands are fused element by element.
struct ExprTerminal {}
private enum arithmetic(string op) = op == "+" || op == "-" || op == "*" || op == "/";
mixin template ExpressionOperators()
{
    import std.traits : Unqual;
    import DSLtk : BinExpr, UnaryExpr;
    auto opBinary(string op, R)(R rhs) if (op == "+" || op == "-" || op == "*" || op == "/")
    {
        alias L = Unqual!(typeof(this));
        return BinExpr!(op, L, R)(this, rhs);
    }
    auto opBinaryRight(string op, L)(L lhs) if (op == "+" || op == "-" || op == "*" || op == "/")
    {
        alias R = Unqual!(typeof(this));
        return BinExpr!(op, L, R)(lhs, this);
    }
    auto opUnary(string op)() if (op == "-" || op == "+")
    {
        alias E = Unqual!(typeof(this));
        return UnaryExpr!(op, E)(this);
    }
}
struct Terminal(T)
{
    T value;
    T expr_value() { return value; }
    mixin ExpressionOperators;
}
auto terminal(T)(T value) { return Terminal!T(value); }
private auto operandValue(T)(auto ref T value)
{
    static if (__traits(hasMember, T, "expr_value")) return value.expr_value();
    else static if (__traits(hasMember, T, "eval")) return value.eval();
    else return value;
}
private enum indexed(T) = __traits(hasMember, T, "eval_at") || isArray!T ||
    ( __traits(hasMember, T, "expr_value") && isArray!(typeof(T.init.expr_value())) );
private auto operandAt(T)(auto ref T value, size_t i)
{
    static if (__traits(hasMember, T, "eval_at")) return value.eval_at(i);
    else static if (indexed!T) return operandValue(value)[i];
    else return operandValue(value);
}
private size_t operandLength(T)(auto ref T value)
{
    static if (__traits(hasMember, T, "eval_at")) return value.length();
    else return operandValue(value).length;
}
struct BinExpr(string op, L, R) if (arithmetic!op)
{
    L lhs;
    R rhs;
    mixin ExpressionOperators;
    static if (indexed!L || indexed!R)
    {
        size_t length()
        {
            static if (indexed!L && indexed!R)
            {
                auto n = operandLength(lhs);
                if (n != operandLength(rhs)) throw new Exception("DSLtk: expression array lengths differ");
                return n;
            }
            else static if (indexed!L) return operandLength(lhs);
            else return operandLength(rhs);
        }
        auto eval_at(size_t i) { return mixin("operandAt(lhs, i) " ~ op ~ " operandAt(rhs, i)"); }
        auto eval()
        {
            alias T = typeof(eval_at(0));
            auto result = new T[length()];
            foreach (i; 0 .. result.length) result[i] = eval_at(i);
            return result;
        }
    }
    else auto eval() { return mixin("operandValue(lhs) " ~ op ~ " operandValue(rhs)"); }
}
struct UnaryExpr(string op, E) if (op == "-" || op == "+")
{
    E operand;
    mixin ExpressionOperators;
    static if (indexed!E)
    {
        size_t length() { return operandLength(operand); }
        auto eval_at(size_t i) { return mixin(op ~ "operandAt(operand, i)"); }
        auto eval()
        {
            alias T = typeof(eval_at(0));
            auto result = new T[length()];
            foreach (i; 0 .. result.length) result[i] = eval_at(i);
            return result;
        }
    }
    else auto eval() { return mixin(op ~ "operandValue(operand)"); }
}

struct LiteralEntry(string text, F)
{
    enum suffix = text;
    F handler;
}
auto lit(string suffix, F)(F fn)
{
    static assert(suffix.length > 0, "literal suffix must not be empty");
    return LiteralEntry!(suffix, F)(fn);
}
struct LiteralSet(E...)
{
    Tuple!E entries;
    real parse(string input)
    {
        static foreach (i; 0 .. E.length)
        {
            if (input.length > E[i].suffix.length && input[$ - E[i].suffix.length .. $] == E[i].suffix)
                return entries[i].handler(to!real(input[0 .. $ - E[i].suffix.length]));
        }
        throw new Exception("DSLtk.LiteralSet.parse: no suffix matched for '" ~ input ~ "'");
    }
}
auto literal_set(E...)(E entries) { return LiteralSet!E(tuple(entries)); }

struct MemoizedCallable(K, V, F)
{
    F fn;
    private V[K] cache;
    V opCall(K key)
    {
        if (auto found = key in cache) return *found;
        auto value = fn(key);
        cache[key] = value;
        return value;
    }
    void clear_cache() { cache = null; }
}
auto memoize(K, V, F)(F fn) { return MemoizedCallable!(K, V, F)(fn); }

/// Optional F keeps functors concrete; Lazy!T accepts delegates/functions.
struct Lazy(T, F = T delegate())
{
    private F producer;
    private Nullable!T cached;
    this(F fn) { producer = fn; }
    static if (is(F == T delegate()))
        this(T function() fn) { producer = () => fn(); }
    ref T get() return
    {
        if (cached.isNull) cached = producer();
        return cached.get;
    }
    ref T force() return { return get(); }
    bool is_computed() const { return !cached.isNull; }
}
auto make_lazy(F)(F fn) { return Lazy!(typeof(fn()), F)(fn); }

struct Maybe(T)
{
    private Nullable!T data;
    this(T value) { data = value; }
    this(Nullable!T value) { data = value; }
    bool has_value() const { return !data.isNull; }
    Nullable!T as_optional() { return data; }
    ref T value() return { return data.get; }
    auto map(F)(F fn)
    {
        alias U = typeof(fn(T.init));
        return data.isNull ? Maybe!U.init : Maybe!U(fn(data.get));
    }
    auto flat_map(F)(F fn)
    {
        alias R = typeof(fn(T.init));
        return data.isNull ? R.init : fn(data.get);
    }
    Maybe filter(P)(P pred) { return !data.isNull && pred(data.get) ? this : Maybe.init; }
    T or_else(T fallback) { return data.isNull ? fallback : data.get; }
}
auto map(T, F)(Nullable!T opt, F fn)
{
    alias U = typeof(fn(T.init));
    return opt.isNull ? Nullable!U.init : Nullable!U(fn(opt.get));
}
auto flat_map(T, F)(Nullable!T opt, F fn)
{
    alias R = typeof(fn(T.init));
    return opt.isNull ? R.init : fn(opt.get);
}
auto filter(T, P)(Nullable!T opt, P pred) { return !opt.isNull && pred(opt.get) ? opt : Nullable!T.init; }
T or_else(T)(Nullable!T opt, T fallback) { return opt.isNull ? fallback : opt.get; }

/// Explicitly tagged storage also supports identical success/error types.
struct Result(T, E)
{
    private bool ok_;
    private T value_;
    private E error_;
    this(T value) { ok_ = true; value_ = value; }
    static if (!is(T == E)) this(E error) { error_ = error; }
    this(OkWrap!T value) { ok_ = true; value_ = value.value; }
    this(ErrWrap!E error) { error_ = error.error; }
    bool is_ok() const { return ok_; }
    bool is_err() const { return !ok_; }
    T unwrap()
    {
        if (!ok_) throw new Exception("DSLtk.Result.unwrap called on Err");
        return value_;
    }
    E unwrap_err()
    {
        if (ok_) throw new Exception("DSLtk.Result.unwrap_err called on Ok");
        return error_;
    }
    T unwrap_or(T fallback) { return ok_ ? value_ : fallback; }
    auto map(F)(F fn)
    {
        alias R = Result!(typeof(fn(T.init)), E);
        return ok_ ? R.from_ok(fn(value_)) : R.from_err(error_);
    }
    auto and_then(F)(F fn)
    {
        alias R = typeof(fn(T.init));
        return ok_ ? fn(value_) : R.from_err(error_);
    }
    auto map_err(F)(F fn)
    {
        alias R = Result!(T, typeof(fn(E.init)));
        return ok_ ? R.from_ok(value_) : R.from_err(fn(error_));
    }
    static Result from_ok(T value) { return Result(Ok(value)); }
    static Result from_err(E error) { return Result(Err(error)); }
}
struct OkWrap(T) { T value; }
struct ErrWrap(E) { E error; }
auto Ok(T)(T value) { return OkWrap!T(value); }
auto Err(E)(E error) { return ErrWrap!E(error); }

// Functional parsers. Combinators retain their complete types in the tree.
struct ParsecInput
{
    string source;
    size_t pos;
    char peek() const { return eof() ? '\0' : source[pos]; }
    bool eof() const { return pos >= source.length; }
    char consume() { return eof() ? '\0' : source[pos++]; }
    string get_span(size_t start) const { return source[start .. pos]; }
}
Result!(string, string) load_parse_input_file(string path)
{
    import std.file : readText;
    try { return Result!(string, string).from_ok(readText(path)); }
    catch (Exception e) { return Result!(string, string).from_err("DSLtk.load_parse_input_file: " ~ e.msg); }
}
enum ParseFailureKind { Soft, Committed }
struct ParseError
{
    size_t pos;
    ParseFailureKind kind = ParseFailureKind.Soft;
    string[] expected;
}
struct ExpectedResult(T)
{
    alias value_type = T;
    Nullable!T value;
    ParseError error;
    this(T result) { value = result; }
    static ExpectedResult failure(size_t pos, ParseFailureKind kind = ParseFailureKind.Soft,
                                  string[] expected = null)
    {
        ExpectedResult result;
        result.error = ParseError(pos, kind, expected);
        return result;
    }
    bool opCast(T : bool)() const { return !value.isNull; }
    ref T get() return { return value.get; }
}
private ExpectedResult!T parseFailure(T)(ParseError error)
{
    return ExpectedResult!T.failure(error.pos, error.kind, error.expected);
}
void merge_error(ref ParseError dst, ParseError src)
{
    if (src.pos > dst.pos) { dst = src; return; }
    if (src.pos < dst.pos) return;
    if (src.kind == ParseFailureKind.Committed) dst.kind = src.kind;
    foreach (label; src.expected)
    {
        bool found;
        foreach (existing; dst.expected) found |= label == existing;
        if (!found) dst.expected ~= label;
    }
}
ExpectedResult!T fail_expected(T)(ParsecInput input, string label,
                                 ParseFailureKind kind = ParseFailureKind.Soft)
{
    return ExpectedResult!T.failure(input.pos, kind, [label]);
}

struct Parser(T, F = ExpectedResult!T delegate(ref ParsecInput))
{
    alias value_type = T;
    F parse;
    ExpectedResult!T opCall(ref ParsecInput input) { return parse(input); }
    auto opBinary(string op, U, G)(Parser!(U, G) other) if (op == "&" || op == "|")
    {
        static if (op == "&") return parser(SequenceParser!(typeof(this), typeof(other))(this, other));
        else
        {
            static assert(is(T == U), "choice alternatives must produce the same type");
            return parser(ChoiceParser!(typeof(this), typeof(other))(this, other));
        }
    }
    auto many()() { return DSLtk.many(this); }
    auto optional()() { return DSLtk.optional(this); }
}
auto parser(F)(F fn)
{
    ParsecInput input;
    alias R = typeof(fn(input));
    return Parser!(R.value_type, F)(fn);
}
private struct SequenceParser(A, B)
{
    A a;
    B b;
    ExpectedResult!(Tuple!(A.value_type, B.value_type)) opCall(ref ParsecInput input)
    {
        alias T = Tuple!(A.value_type, B.value_type);
        auto save = input.pos;
        auto ra = a(input);
        if (!ra) return parseFailure!T(ra.error);
        auto rb = b(input);
        if (!rb)
        {
            auto error = rb.error;
            if (input.pos != save) error.kind = ParseFailureKind.Committed;
            input.pos = save;
            return parseFailure!T(error);
        }
        return ExpectedResult!T(tuple(ra.get, rb.get));
    }
}
private struct ChoiceParser(A, B)
{
    A a;
    B b;
    ExpectedResult!(A.value_type) opCall(ref ParsecInput input)
    {
        auto save = input.pos;
        auto ra = a(input);
        if (ra || ra.error.kind == ParseFailureKind.Committed) return ra;
        input.pos = save;
        auto rb = b(input);
        if (!rb) merge_error(rb.error, ra.error);
        return rb;
    }
}
private struct ManyParser(P)
{
    P p;
    ExpectedResult!(P.value_type[]) opCall(ref ParsecInput input)
    {
        alias T = P.value_type[];
        T values;
        while (true)
        {
            auto save = input.pos;
            auto result = p(input);
            if (!result)
            {
                if (result.error.kind == ParseFailureKind.Committed) return parseFailure!T(result.error);
                input.pos = save;
                break;
            }
            if (input.pos == save)
                return ExpectedResult!T.failure(save, ParseFailureKind.Committed, ["<non-empty repetition>"]);
            values ~= result.get;
        }
        return ExpectedResult!T(values);
    }
}
auto many(P)(P p) { return parser(ManyParser!P(p)); }
private struct OptionalParser(P)
{
    P p;
    ExpectedResult!(Nullable!(P.value_type)) opCall(ref ParsecInput input)
    {
        alias T = Nullable!(P.value_type);
        auto save = input.pos;
        auto result = p(input);
        if (result) return ExpectedResult!T(T(result.get));
        if (result.error.kind == ParseFailureKind.Committed) return parseFailure!T(result.error);
        input.pos = save;
        return ExpectedResult!T(T.init);
    }
}
auto optional(P)(P p) { return parser(OptionalParser!P(p)); }
private struct TryParser(P)
{
    P p;
    ExpectedResult!(P.value_type) opCall(ref ParsecInput input)
    {
        auto save = input.pos;
        auto result = p(input);
        if (!result) { input.pos = save; result.error.kind = ParseFailureKind.Soft; }
        return result;
    }
}
auto try_parse(P)(P p) { return parser(TryParser!P(p)); }
private struct LabeledParser(P)
{
    P p;
    string label;
    ExpectedResult!(P.value_type) opCall(ref ParsecInput input)
    {
        auto result = p(input);
        if (!result) result.error.expected = [label];
        return result;
    }
}
auto labeled(P)(P p, string label) { return parser(LabeledParser!P(p, label)); }
struct ParseOutcome(T)
{
    Nullable!T value;
    ParseError error;
}
ParseOutcome!(P.value_type) run_parser(P)(P p, string source)
{
    ParsecInput input = ParsecInput(source);
    auto result = p(input);
    if (!result) return ParseOutcome!(P.value_type)(Nullable!(P.value_type).init, result.error);
    if (!input.eof()) return ParseOutcome!(P.value_type)(Nullable!(P.value_type).init,
        ParseError(input.pos, ParseFailureKind.Committed, ["<end-of-input>"]));
    return ParseOutcome!(P.value_type)(result.value);
}
private struct CharParser
{
    char ch;
    ExpectedResult!char opCall(ref ParsecInput input)
    {
        if (!input.eof() && input.peek() == ch) return ExpectedResult!char(input.consume());
        return fail_expected!char(input, to!string(ch));
    }
}
auto ch(char c) { return parser(CharParser(c)); }
private struct SatisfyParser(P)
{
    P pred;
    string label;
    ExpectedResult!char opCall(ref ParsecInput input)
    {
        if (!input.eof() && pred(input.peek())) return ExpectedResult!char(input.consume());
        return fail_expected!char(input, label);
    }
}
auto satisfy(P)(P pred, string label) { return parser(SatisfyParser!P(pred, label)); }

struct ParserStage
{
    size_t begin;
    size_t end;
    string span;
}
auto parse_with_action(P, S, F)(P p, ref ParsecInput input, S semantics, F action)
{
    alias T = typeof(action(semantics, ParserStage.init));
    auto begin = input.pos;
    auto result = p(input);
    if (!result) return Nullable!T.init;
    return Nullable!T(action(semantics, ParserStage(begin, input.pos, input.get_span(begin))));
}
struct ProductionHandler(string id, F)
{
    enum identifier = id;
    F fn;
}
auto production(string id, F)(F fn) { return ProductionHandler!(id, F)(fn); }

// Stateful tasks use delegates for heterogeneous runtime task chains.
enum TaskPolicy { StopOnError, ContinueOnError }
struct TaskState
{
    Result!(string, string)[string] results;
    bool suspended;
}
struct Task
{
    string name;
    Result!(string, string) delegate(ref TaskState) run_fn;
    this(string name, Result!(string, string) delegate(ref TaskState) fn) { this.name = name; run_fn = fn; }
    this(string name, Result!(string, string) function(ref TaskState) fn)
    {
        this.name = name;
        run_fn = (ref TaskState state) => fn(state);
    }
    Result!(string, string) run(ref TaskState state) { return run_fn(state); }
    auto opBinary(string op)(Task other) if (op == "|") { return TaskChain([this, other]); }
}
struct TaskChain
{
    Task[] tasks;
    TaskPolicy policy = TaskPolicy.StopOnError;
    auto opBinary(string op)(Task task) if (op == "|") { return TaskChain(tasks ~ task, policy); }
}
ref TaskState run(A...)(return ref TaskState state, TaskChain chain, A args)
{
    foreach (task; chain.tasks)
    {
        auto result = task.run(state);
        state.results[task.name] = result;
        if (result.is_err() && chain.policy == TaskPolicy.StopOnError) break;
        if (state.suspended) break;
    }
    return state;
}
void suspend(ref TaskState state) { state.suspended = true; }
void upon(F)(ref TaskState state, F event, Task task)
{
    if (event()) state.results[task.name] = task.run(state);
}

// PEG patterns: compile at CTFE for string-template rules, runtime otherwise.
struct CompiledPattern
{
    struct Atom
    {
        bool any;
        bool is_class;
        bool negate;
        char ch;
        string cls;
        char quant;
        bool accepts(char c) const
        {
            if (any) return true;
            if (is_class) return negate != classTest(cls, c);
            return ch == c;
        }
    }
    Atom[] atoms;
    bool valid = true;
    string error;
    static CompiledPattern compile(string pat)
    {
        CompiledPattern result;
        size_t i;
        while (i < pat.length)
        {
            if (pat[i] == '^' || pat[i] == '$') { ++i; continue; }
            Atom a;
            if (pat[i] == '.') { a.any = true; ++i; }
            else if (pat[i] == '[')
            {
                size_t close = i + 1;
                while (close < pat.length && pat[close] != ']') ++close;
                if (close == pat.length)
                {
                    result.valid = false;
                    result.error = "unterminated character class";
                    return result;
                }
                a.is_class = true;
                a.cls = pat[i + 1 .. close];
                if (a.cls.length && a.cls[0] == '^') { a.negate = true; a.cls = a.cls[1 .. $]; }
                i = close + 1;
            }
            else a.ch = pat[i++];
            if (i < pat.length && (pat[i] == '*' || pat[i] == '+' || pat[i] == '?' || pat[i] == '!'))
                a.quant = pat[i++];
            result.atoms ~= a;
        }
        return result;
    }
    Nullable!size_t match_at(string text, size_t start) const
    {
        if (!valid || start > text.length) return Nullable!size_t.init;
        size_t pos = start;
        foreach (ref a; atoms)
        {
            switch (a.quant)
            {
                case '*': case '+':
                    auto begin = pos;
                    while (pos < text.length && a.accepts(text[pos])) ++pos;
                    if (a.quant == '+' && pos == begin) return Nullable!size_t.init;
                    break;
                case '?':
                    if (pos < text.length && a.accepts(text[pos])) ++pos;
                    break;
                case '!':
                    if (pos < text.length && a.accepts(text[pos])) return Nullable!size_t.init;
                    break;
                default:
                    if (pos == text.length || !a.accepts(text[pos])) return Nullable!size_t.init;
                    ++pos;
                    break;
            }
        }
        return Nullable!size_t(pos);
    }
}

struct PEGChannel
{
    string name = "@DEFAULT";
    bool is_ignore() const { return name == "@IGNORE"; }
}
enum PEGIgnoreChannel = PEGChannel("@IGNORE");
enum PEGDefaultChannel = PEGChannel("@DEFAULT");
PEGChannel new_peg_channel(string name)
{
    if (!name.length || name[0] != '@') throw new Exception("DSLtk: channel names must begin with '@'");
    return PEGChannel(name);
}
struct PEGMatch
{
    string value;
    size_t begin;
    size_t end;
    PEGRule rule;
    PEGChannel channel;
    void* user_data;
    size_t line = 1;
    size_t column = 1;
    bool ignored;
    size_t length() const { return end - begin; }
}
alias PEGSemanticAction = void delegate(ref PEGMatch);
struct PEGArm
{
    PEGRule rule;
    PEGSemanticAction action;
    bool is_wildcard;
    bool is_channel;
    PEGChannel channel;
}
private PEGSemanticAction semanticAction(F)(F fn)
{
    static if (is(F == typeof(null))) return null;
    else return (ref PEGMatch m) { fn(m); };
}
class PEGRule
{
    string pattern_text;
    CompiledPattern compiled;
    PEGSemanticAction semantic_action;
    PEGChannel channel;
    bool must_fail;
    bool fire_on_neg;
    PEGRule if_succeed;
    size_t order;
    Nullable!size_t try_match(string text, size_t pos)
    {
        auto end = compiled.match_at(text, pos);
        if (!must_fail) return end;
        return end.isNull && pos < text.length ? Nullable!size_t(pos + 1) : Nullable!size_t.init;
    }
    PEGArm opCall(F)(F fn) { return PEGArm(this, semanticAction(fn)); }
}
struct PEGParseResult
{
    bool success = true;
    size_t offset;
    size_t line = 1;
    size_t column = 1;
    string message;
    bool ok() const { return success; }
    bool failed() const { return !success; }
    size_t error_offset() const { return offset; }
    size_t error_line() const { return line; }
    size_t error_column() const { return column; }
    string error_message() const { return message; }
}
private void lineColAt(string text, size_t pos, out size_t line, out size_t column)
{
    line = column = 1;
    foreach (c; text[0 .. pos])
    {
        if (c == '\n') { ++line; column = 1; }
        else ++column;
    }
}
private PEGMatch makePEGMatch(string text, size_t begin, size_t end, PEGRule rule = null)
{
    PEGMatch m;
    m.value = text[begin .. end];
    m.begin = begin;
    m.end = end;
    m.rule = rule;
    if (rule !is null) { m.channel = rule.channel; m.ignored = rule.channel.is_ignore(); }
    lineColAt(text, begin, m.line, m.column);
    return m;
}
class PEGDefinition
{
    PEGDefinition parent;
    private PEGRule[] local_rules;
    PEGRule add_rule(string text)() { return add_rule!text(null); }
    PEGRule add_rule(string text, F)(F action)
    {
        enum compiled = CompiledPattern.compile(text);
        static assert(compiled.valid, compiled.error ~ " in PEG pattern '" ~ text ~ "'");
        auto rule = new PEGRule;
        rule.pattern_text = text;
        rule.compiled = compiled;
        rule.semantic_action = semanticAction(action);
        rule.order = local_rules.length;
        local_rules ~= rule;
        return rule;
    }
    PEGRule[] rules()
    {
        auto visible = parent is null ? null : parent.rules();
        return visible ~ local_rules;
    }
    PEGParseResult parse(string text)
    {
        auto all = rules();
        size_t pos;
        while (pos < text.length)
        {
            bool matched;
            foreach (rule; all)
            {
                auto end = rule.try_match(text, pos);
                if (end.isNull) continue;
                if (end.get == pos) return failure(text, pos, "PEG rule matched empty input; refusing to loop");
                matched = true;
                if ((!rule.must_fail || rule.fire_on_neg) && rule.semantic_action !is null)
                {
                    auto m = makePEGMatch(text, pos, end.get, rule);
                    rule.semantic_action(m);
                }
                if (rule.if_succeed !is null)
                {
                    auto next = rule.if_succeed.try_match(text, pos);
                    if (!next.isNull && rule.if_succeed.semantic_action !is null)
                    {
                        auto m = makePEGMatch(text, pos, next.get, rule.if_succeed);
                        rule.if_succeed.semantic_action(m);
                    }
                }
                pos = end.get;
                break;
            }
            if (!matched) return failure(text, pos, "no PEG rule matched at offset " ~ to!string(pos));
        }
        PEGParseResult result;
        result.offset = pos;
        return result;
    }
    private static PEGParseResult failure(string text, size_t pos, string message)
    {
        PEGParseResult result;
        result.success = false;
        result.offset = pos;
        result.message = message;
        lineColAt(text, pos, result.line, result.column);
        return result;
    }
}
PEGDefinition create_peg_definition() { return new PEGDefinition; }
PEGDefinition derive_peg_definition(PEGDefinition parent)
{
    auto child = new PEGDefinition;
    child.parent = parent;
    return child;
}

class PEGMatcher
{
    PEGDefinition def;
    string text;
    size_t pos;
    private enum FilterMode { All, Only, Exclude }
    private FilterMode mode;
    private string[] channels;
    private PEGArm[] arms;
    this(PEGDefinition def, string text) { this.def = def; this.text = text; }
    bool is_spent() const { return pos >= text.length; }
    size_t position() const { return pos; }
    void close() { arms = null; }
    PEGMatcher only(string channel) { mode = FilterMode.Only; channels = [channel]; return this; }
    PEGMatcher include(string channel)
    {
        if (mode == FilterMode.Exclude) mode = FilterMode.Only;
        channels ~= channel;
        return this;
    }
    PEGMatcher exclude(string channel)
    {
        if (mode != FilterMode.Exclude) { mode = FilterMode.Exclude; channels = null; }
        channels ~= channel;
        return this;
    }
    bool channel_allowed(string name) const
    {
        if (mode == FilterMode.All) return true;
        bool found;
        foreach (channel; channels) found |= channel == name;
        return mode == FilterMode.Only ? found : !found;
    }
    PEGArm wildcard(F)(F fn)
    {
        PEGArm arm;
        arm.is_wildcard = true;
        arm.action = semanticAction(fn);
        return arm;
    }
    /// Channel-wide arm, subject to the same filters as rule arms.
    PEGArm on_channel(F)(PEGChannel channel, F fn)
    {
        PEGArm arm;
        arm.is_channel = true;
        arm.channel = channel;
        arm.action = semanticAction(fn);
        return arm;
    }
    PEGMatcher opBinary(string op)(PEGArm arm) if (op == "<<")
    {
        arms ~= arm;
        step();
        return this;
    }
    private void step()
    {
        if (!arms.length || is_spent()) return;
        skip_ignored();
        if (is_spent()) { arms = null; return; }
        foreach (arm; arms)
        {
            if (arm.is_wildcard) continue;
            if (arm.is_channel)
            {
                foreach (rule; def.rules())
                {
                    if (rule.channel != arm.channel || !channel_allowed(rule.channel.name)) continue;
                    auto end = rule.try_match(text, pos);
                    if (!end.isNull) { fire(arm.action, rule, end.get); arms = null; return; }
                }
            }
            else if (arm.rule !is null && channel_allowed(arm.rule.channel.name))
            {
                auto end = arm.rule.try_match(text, pos);
                if (!end.isNull) { fire(arm.action, arm.rule, end.get); arms = null; return; }
            }
        }
        foreach (arm; arms)
            if (arm.is_wildcard)
            {
                auto m = makePEGMatch(text, pos, pos + 1);
                if (arm.action !is null) arm.action(m);
                ++pos;
                arms = null;
                return;
            }
    }
    private void skip_ignored()
    {
        while (!is_spent())
        {
            bool progressed;
            foreach (rule; def.rules())
            {
                if (!rule.channel.is_ignore()) continue;
                auto end = rule.try_match(text, pos);
                if (!end.isNull && end.get > pos) { pos = end.get; progressed = true; break; }
            }
            if (!progressed) break;
        }
    }
    private void fire(PEGSemanticAction action, PEGRule rule, size_t end)
    {
        if (end == pos) throw new Exception("DSLtk.PEGMatcher: rule matched empty input");
        auto m = makePEGMatch(text, pos, end, rule);
        pos = end;
        if (action !is null) action(m);
        else if (rule.semantic_action !is null) rule.semantic_action(m);
    }
}
PEGMatcher peg_new_matcher(PEGDefinition def, string text) { return new PEGMatcher(def, text); }

// PEG combinators over the same concrete Parser templates.
private struct SpanParser(P)
{
    P p;
    ExpectedResult!PEGMatch opCall(ref ParsecInput input)
    {
        auto begin = input.pos;
        auto result = p(input);
        if (!result) return parseFailure!PEGMatch(result.error);
        return ExpectedResult!PEGMatch(makePEGMatch(input.source, begin, input.pos));
    }
}
auto as_span_parser(P)(P p) { return parser(SpanParser!P(p)); }
auto peg_seq(A, B, R...)(A a, B b, R rest)
{
    auto ab = as_span_parser(a & b);
    static if (!R.length) return ab;
    else return peg_seq(ab, rest);
}
auto peg_choice(A, B, R...)(A a, B b, R rest)
{
    auto ab = try_parse(a) | try_parse(b);
    static if (!R.length) return ab;
    else return peg_choice(ab, rest);
}
auto peg_opt(P)(P p) { return optional(try_parse(p)); }
auto peg_many(P)(P p) { return many(try_parse(p)); }
auto peg_many1(P)(P p) { return p & many(try_parse(p)); }
private struct LookaheadParser(bool positive, P)
{
    P p;
    ExpectedResult!bool opCall(ref ParsecInput input)
    {
        auto save = input.pos;
        auto result = p(input);
        input.pos = save;
        static if (positive)
        {
            if (!result)
                return ExpectedResult!bool.failure(result.error.pos, ParseFailureKind.Soft, result.error.expected);
        }
        else if (result) return fail_expected!bool(input, "negative lookahead");
        return ExpectedResult!bool(true);
    }
}
auto peg_and(P)(P p) { return parser(LookaheadParser!(true, P)(p)); }
auto peg_not(P)(P p) { return parser(LookaheadParser!(false, P)(p)); }
private struct RuleParser
{
    PEGRule rule;
    ExpectedResult!PEGMatch opCall(ref ParsecInput input)
    {
        auto begin = input.pos;
        auto end = rule.try_match(input.source, begin);
        if (end.isNull) return fail_expected!PEGMatch(input, rule.pattern_text);
        input.pos = end.get;
        return ExpectedResult!PEGMatch(makePEGMatch(input.source, begin, end.get, rule));
    }
}
auto peg_rule(PEGRule rule) { return parser(RuleParser(rule)); }
private struct ChannelParser(P)
{
    PEGChannel channel;
    P p;
    ExpectedResult!PEGMatch opCall(ref ParsecInput input)
    {
        auto result = as_span_parser(p)(input);
        if (result)
        {
            result.get.channel = channel;
            result.get.ignored = channel.is_ignore();
        }
        return result;
    }
}
auto peg_channel(P)(PEGChannel channel, P p) { return parser(ChannelParser!P(channel, p)); }
auto peg_channel(P)(string channel, P p) { return peg_channel(new_peg_channel(channel), p); }

// Native enum reflection. Optional UDAs replace C++ customization specializations.
/// Attach @(EnumOptions(true)) to a flags enum; prefix_length trims names.
struct EnumOptions
{
    bool is_flags;
    size_t prefix_length;
}
/// Attach @(EnumNames(["Member": "display-name"])) to customize member names.
struct EnumNames { string[string] names; }
private enum integralEnum(E) = is(E == enum) && isIntegral!(OriginalType!E);
private EnumOptions enumOptions(E)()
{
    import std.traits : getUDAs;
    alias options = getUDAs!(E, EnumOptions);
    static assert(options.length <= 1, "at most one EnumOptions UDA is allowed");
    static if (options.length) return options[0];
    else return EnumOptions.init;
}
enum is_flags(E) = is(E == enum) && enumOptions!E().is_flags;
alias is_flags_v = is_flags;
alias underlying_type_t(E) = OriginalType!E;
alias underlying_type(E) = OriginalType!E;
enum is_magic_enum_supported = true;

private string declaredEnumName(E)(E value)
{
    import std.traits : getUDAs;
    alias custom = getUDAs!(E, EnumNames);
    static assert(custom.length <= 1, "at most one EnumNames UDA is allowed");
    static foreach (member; __traits(allMembers, E))
    {
        if (value == __traits(getMember, E, member))
        {
            static if (custom.length)
            {
                enum names = custom[0].names;
                if (auto found = member in names) return *found;
            }
            enum prefix = enumOptions!E().prefix_length;
            static assert(prefix <= member.length, "enum prefix_length exceeds a member's name length");
            return member[prefix .. $];
        }
    }
    return "";
}
private E[] reflectedEnumValues(E)() if (integralEnum!E)
{
    E[] values;
    static foreach (member; EnumMembers!E)
    {{
        bool duplicate;
        foreach (v; values) duplicate |= v == member;
        auto n = cast(OriginalType!E) member;
        static if (is_flags!E) bool included = n > 0 && (n & (n - 1)) == 0;
        else bool included = true;
        if (!duplicate && included && declaredEnumName(member).length) values ~= member;
    }}
    // Insertion sort is CTFE-friendly, preserves first-declared aliases.
    foreach (i; 1 .. values.length)
    {
        auto value = values[i];
        size_t j = i;
        while (j && cast(OriginalType!E) values[j - 1] > cast(OriginalType!E) value)
        { values[j] = values[j - 1]; --j; }
        values[j] = value;
    }
    return values;
}
private template EnumData(E) if (integralEnum!E)
{
    enum values = reflectedEnumValues!E();
    enum names = () {
        string[] result;
        foreach (v; values) result ~= declaredEnumName(v);
        return result;
    }();
}
auto enum_values(E)() if (integralEnum!E)
{
    E[EnumData!E.values.length] values;
    values[] = EnumData!E.values[];
    return values;
}
auto enum_names(E)() if (integralEnum!E)
{
    string[EnumData!E.names.length] names;
    names[] = EnumData!E.names[];
    return names;
}
size_t enum_count(E)() if (integralEnum!E) { return EnumData!E.values.length; }
string enum_type_name(E)() if (is(E == enum)) { return __traits(identifier, E); }
string enum_name(E)(E value) if (integralEnum!E)
{
    foreach (i, v; EnumData!E.values) if (v == value) return EnumData!E.names[i];
    return "";
}
string enum_name(alias value)() if (is(typeof(value) == enum)) { return declaredEnumName(value); }
auto enum_integer(E)(E value) if (integralEnum!E) { return cast(OriginalType!E) value; }
alias enum_underlying = enum_integer;
Nullable!size_t enum_index(E)(E value) if (integralEnum!E)
{
    foreach (i, v; EnumData!E.values) if (v == value) return Nullable!size_t(i);
    return Nullable!size_t.init;
}
Nullable!size_t enum_index(alias value)() { return enum_index(value); }
E enum_value(E)(size_t index) if (integralEnum!E)
{
    if (index >= EnumData!E.values.length) throw new Exception("DSLtk.enum_value: index out of range");
    return EnumData!E.values[index];
}
E enum_value(E, size_t index)() if (integralEnum!E)
{
    static assert(index < EnumData!E.values.length, "enum index out of range");
    return EnumData!E.values[index];
}
auto enum_entries(E)() if (integralEnum!E)
{
    Tuple!(E, "value", string, "name")[EnumData!E.values.length] entries;
    foreach (i; 0 .. entries.length)
        entries[i] = typeof(entries[0])(EnumData!E.values[i], EnumData!E.names[i]);
    return entries;
}
private struct CaseInsensitive
{
    bool opCall(char a, char b) const
    {
        if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
        return a == b;
    }
}
enum case_insensitive = CaseInsensitive();
private struct CaseSensitive { bool opCall(char a, char b) const { return a == b; } }
private bool enumNameEquals(P)(string a, string b, P pred)
{
    if (a.length != b.length) return false;
    foreach (i; 0 .. a.length) if (!pred(a[i], b[i])) return false;
    return true;
}
Nullable!E enum_cast(E, P = CaseSensitive)(string name, P pred = P.init) if (integralEnum!E)
{
    foreach (i, n; EnumData!E.names)
        if (enumNameEquals(name, n, pred)) return Nullable!E(EnumData!E.values[i]);
    return Nullable!E.init;
}
Nullable!E enum_cast(E, I)(I integer) if (integralEnum!E && isIntegral!I)
{
    // Compare before casting, avoiding integer truncation and signedness traps.
    foreach (v; EnumData!E.values)
    {
        auto n = cast(OriginalType!E) v;
        import std.traits : isUnsigned;
        static if (isUnsigned!I != isUnsigned!(OriginalType!E))
        {
            static if (isUnsigned!I) { if (n < 0) continue; }
            else { if (integer < 0) continue; }
            if (cast(ulong) n == cast(ulong) integer) return Nullable!E(v);
        }
        else if (n == integer) return Nullable!E(v);
    }
    return Nullable!E.init;
}
bool enum_contains(E, V)(V value) if (integralEnum!E) { return !enum_cast!E(value).isNull; }
bool enum_contains(E, P)(string value, P pred) if (integralEnum!E) { return !enum_cast!E(value, pred).isNull; }
bool enum_reflected(E, I)(I value) if (integralEnum!E && isIntegral!I) { return enum_contains!E(value); }

private Nullable!E adjacentEnum(E)(E value, ptrdiff_t offset, bool circular) if (integralEnum!E)
{
    auto index = enum_index(value);
    if (index.isNull) return Nullable!E.init;
    auto size = cast(ptrdiff_t) enum_count!E();
    auto next = cast(ptrdiff_t) index.get;
    if (circular) next = ((next + offset % size) % size + size) % size;
    else
    {
        if (offset < -next || offset >= size - next) return Nullable!E.init;
        next += offset;
    }
    return Nullable!E(enum_value!E(cast(size_t) next));
}
Nullable!E enum_next_value(E)(E value, ptrdiff_t offset = 1) { return adjacentEnum(value, offset, false); }
Nullable!E enum_prev_value(E)(E value, ptrdiff_t offset = 1)
{
    if (offset == ptrdiff_t.min) return Nullable!E.init;
    return adjacentEnum(value, -offset, false);
}
Nullable!E enum_next_value_circular(E)(E value, ptrdiff_t offset = 1) { return adjacentEnum(value, offset, true); }
Nullable!E enum_prev_value_circular(E)(E value, ptrdiff_t offset = 1)
{
    if (offset == ptrdiff_t.min)
    {
        if (!enum_count!E()) return Nullable!E.init;
        return adjacentEnum(value, -(offset % cast(ptrdiff_t) enum_count!E()), true);
    }
    return adjacentEnum(value, -offset, true);
}

struct enum_constant(alias v)
{
    enum value = v;
    alias value_type = typeof(v);
}
void enum_for_each(E, F)(F fn) if (integralEnum!E)
{
    static foreach (v; EnumData!E.values) fn(enum_constant!v());
}
void enum_for_each(E, alias fn)() if (integralEnum!E) { enum_for_each!E(StaticCallable!fn()); }
/// Handler receives a compile-time constant wrapper; unknown values throw.
auto enum_switch(F, E)(F fn, E value) if (integralEnum!E)
{
    static foreach (v; EnumData!E.values)
        if (value == v) return fn(enum_constant!v());
    throw new Exception("DSLtk.enum_switch: unrecognized enum value");
}
auto enum_switch(F, E, R)(F fn, E value, R fallback) if (integralEnum!E)
{
    static foreach (v; EnumData!E.values)
        if (value == v) return fn(enum_constant!v());
    return fallback;
}
auto enum_switch(alias fn, E)(E value) if (integralEnum!E)
{ return enum_switch(StaticCallable!fn(), value); }
auto enum_switch(alias fn, E, R)(E value, R fallback) if (integralEnum!E)
{ return enum_switch(StaticCallable!fn(), value, fallback); }
Nullable!ulong enum_fuse(E...)(E values)
{
    static assert(E.length > 0, "enum_fuse needs at least one enum");
    ulong result;
    static foreach (i; 0 .. E.length)
    {{
        auto index = enum_index(values[i]);
        if (index.isNull) return Nullable!ulong.init;
        enum count = enum_count!(E[i])();
        if (result > (ulong.max - index.get) / count)
            throw new Exception("DSLtk.enum_fuse: combined index exceeds ulong");
        result = result * count + index.get;
    }}
    return Nullable!ulong(result);
}

private ulong enumFlagMask(E)() if (integralEnum!E)
{
    ulong mask;
    foreach (v; EnumMembers!E)
    {
        auto n = cast(ulong) v;
        if (n && (n & (n - 1)) == 0 && declaredEnumName(v).length) mask |= n;
    }
    return mask;
}
bool enum_flags_contains(E)(E value) if (integralEnum!E)
{
    auto bits = cast(ulong) value;
    return bits != 0 && (bits & ~enumFlagMask!E()) == 0;
}
bool enum_flags_test(E)(E value, E flags) if (integralEnum!E)
{ return (enum_integer(value) & enum_integer(flags)) == enum_integer(flags) && enum_integer(flags) != 0; }
bool enum_flags_test_any(E)(E value, E flags) if (integralEnum!E)
{ return (enum_integer(value) & enum_integer(flags)) != 0; }
string enum_flags_name(E)(E value) if (integralEnum!E)
{
    if (!enum_flags_contains(value)) return "";
    string result;
    // EnumMembers is declaration ordered; sort names via the common reflection.
    foreach (v; reflectedEnumValues!E())
    {
        auto n = cast(ulong) v;
        if (!n || (n & (n - 1)) != 0 || (cast(ulong) value & n) == 0) continue;
        if (result.length) result ~= "|";
        result ~= declaredEnumName(v);
    }
    return result;
}
Nullable!E enum_flags_cast(E, P = CaseSensitive)(string names, P pred = P.init) if (integralEnum!E)
{
    ulong result;
    while (names.length)
    {
        size_t end;
        while (end < names.length && names[end] != '|') ++end;
        bool found;
        foreach (v; EnumMembers!E)
        {
            auto n = cast(ulong) v;
            if (!n || (n & (n - 1)) != 0) continue;
            if (enumNameEquals(names[0 .. end], declaredEnumName(v), pred))
            { result |= n; found = true; break; }
        }
        if (!found) return Nullable!E.init;
        if (end == names.length) break;
        names = names[end + 1 .. $];
        if (!names.length) return Nullable!E.init;
    }
    return result ? Nullable!E(cast(E) result) : Nullable!E.init;
}
Nullable!E enum_flags_cast(E, I)(I integer) if (integralEnum!E && isIntegral!I)
{
    auto value = cast(E) integer;
    return cast(ulong) value == cast(ulong) integer && enum_flags_contains(value)
        ? Nullable!E(value) : Nullable!E.init;
}

// Enum-indexed, fixed-storage containers. D enums already support bitwise ops.
struct EnumArray(E, V) if (integralEnum!E)
{
    V[enum_count!E()] data;
    size_t length() const { return data.length; }
    ref V opIndex(E key) return { return data[checkedEnumIndex(key)]; }
    ref const(V) opIndex(E key) const return { return data[checkedEnumIndex(key)]; }
    V opIndexAssign(V value, E key) { return data[checkedEnumIndex(key)] = value; }
    V[] opSlice() return { return data[]; }
    const(V)[] opSlice() const return { return data[]; }
    void fill(V value) { data[] = value; }
}
private size_t checkedEnumIndex(E)(E key)
{
    auto index = enum_index(key);
    if (index.isNull) throw new Exception("DSLtk: unrecognized enum container key");
    return index.get;
}
struct EnumBitset(E) if (integralEnum!E)
{
    private bool[enum_count!E()] bits;
    bool opIndex(E key) const { return bits[checkedEnumIndex(key)]; }
    bool opIndexAssign(bool value, E key) { return bits[checkedEnumIndex(key)] = value; }
    bool test(E key) const { return this[key]; }
    void set(E key, bool value = true) { this[key] = value; }
    void reset(E key) { this[key] = false; }
    void reset() { bits[] = false; }
    void set() { bits[] = true; }
    void flip(E key) { this[key] = !this[key]; }
    void flip() { foreach (ref bit; bits) bit = !bit; }
    size_t count() const { size_t n; foreach (bit; bits) n += bit; return n; }
    size_t size() const { return bits.length; }
    bool any() const { return count() != 0; }
    bool none() const { return !any(); }
    bool all() const { return count() == bits.length; }
    auto opBinary(string op)(EnumBitset other) if (op == "&" || op == "|" || op == "^")
    {
        EnumBitset result;
        foreach (i; 0 .. bits.length) result.bits[i] = mixin("bits[i] " ~ op ~ " other.bits[i]");
        return result;
    }
    auto opUnary(string op)() if (op == "~") { auto result = this; result.flip(); return result; }
}
struct EnumSet(E) if (integralEnum!E)
{
    private EnumBitset!E members;
    bool insert(E key) { auto added = !members[key]; members.set(key); return added; }
    bool erase(E key) { auto removed = members[key]; members.reset(key); return removed; }
    bool contains(E key) const { return members[key]; }
    size_t size() const { return members.count(); }
    bool empty() const { return members.none(); }
    void clear() { members.reset(); }
    int opApply(scope int delegate(E) action)
    {
        foreach (key; EnumData!E.values)
            if (members[key]) { auto result = action(key); if (result) return result; }
        return 0;
    }
}

// S-expressions and equality saturation.
struct SExpr
{
    bool is_atom = true;
    string text;
    SExpr[] items;
    bool quoted;
    static SExpr atom(string text, bool quoted = false)
    {
        quoted |= !text.length;
        foreach (char c; text) quoted |= c <= ' ' || c == '(' || c == ')' || c == '"' || c == ';' || c == '\\';
        return SExpr(true, text, null, quoted);
    }
    static SExpr list(SExpr[] items) { return SExpr(false, null, items.dup); }
    this(this) { items = items.dup; }
    string dump() const
    {
        if (!is_atom)
        {
            string result = "(";
            foreach (i, ref item; items) { if (i) result ~= " "; result ~= item.dump(); }
            return result ~ ")";
        }
        bool quote = quoted || !text.length;
        foreach (char c; text) quote |= c <= ' ' || c == '(' || c == ')' || c == '"' || c == ';' || c == '\\';
        if (!quote) return text;
        string result = "\"";
        foreach (char c; text)
        {
            switch (c)
            {
                case '\n': result ~= "\\n"; break;
                case '\r': result ~= "\\r"; break;
                case '\t': result ~= "\\t"; break;
                case '"': result ~= "\\\""; break;
                case '\\': result ~= "\\\\"; break;
                default: result ~= c; break;
            }
        }
        return result ~ "\"";
    }
    bool opEquals(const SExpr other) const
    { return is_atom == other.is_atom && text == other.text && quoted == other.quoted && items == other.items; }
}
private struct SExprReader
{
    string source;
    size_t pos;
    void skip()
    {
        while (pos < source.length)
        {
            if (source[pos] <= ' ') ++pos;
            else if (source[pos] == ';') { while (pos < source.length && source[pos] != '\n') ++pos; }
            else break;
        }
    }
    SExpr read(size_t depth = 0)
    {
        if (depth > 512) throw new Exception("S-expression nesting limit exceeded");
        skip();
        if (pos == source.length) throw new Exception("expected S-expression at " ~ to!string(pos));
        if (source[pos] == ')') throw new Exception("unexpected ')' at " ~ to!string(pos));
        if (source[pos] == '(')
        {
            ++pos;
            SExpr[] items;
            while (true)
            {
                skip();
                if (pos == source.length) throw new Exception("unterminated S-expression list");
                if (source[pos] == ')') { ++pos; return SExpr.list(items); }
                items ~= read(depth + 1);
            }
        }
        if (source[pos] == '"')
        {
            ++pos;
            string text;
            while (pos < source.length)
            {
                char c = source[pos++];
                if (c == '"') return SExpr.atom(text, true);
                if (c == '\\')
                {
                    if (pos == source.length) throw new Exception("unfinished S-expression escape");
                    c = source[pos++];
                    switch (c)
                    {
                        case 'n': c = '\n'; break; case 'r': c = '\r'; break; case 't': c = '\t'; break;
                        case '\\': case '"': break; default: throw new Exception("unknown S-expression escape");
                    }
                }
                text ~= c;
            }
            throw new Exception("unterminated quoted atom");
        }
        auto begin = pos;
        while (pos < source.length && source[pos] > ' ' && source[pos] != '(' && source[pos] != ')' && source[pos] != ';') ++pos;
        return SExpr.atom(source[begin .. pos]);
    }
}
SExpr[] parse_sexpressions(string source)
{
    auto reader = SExprReader(source);
    SExpr[] result;
    reader.skip();
    while (reader.pos < source.length) { result ~= reader.read(); reader.skip(); }
    return result;
}
SExpr parse_sexpr(string source)
{
    auto items = parse_sexpressions(source);
    if (items.length != 1) throw new Exception("expected exactly one S-expression");
    return items[0];
}
SExpr ast_to_sexpr(ASTNode node)
{
    SExpr[] items = [SExpr.atom(node.tag())];
    if (node.is_leaf()) items ~= SExpr.atom(node.value());
    else foreach (child; node.children()) items ~= ast_to_sexpr(child);
    return SExpr.list(items);
}
ASTNode sexpr_to_ast(SExpr expression)
{
    if (expression.is_atom) return ASTNode("atom", expression.text);
    if (!expression.items.length || !expression.items[0].is_atom)
        throw new Exception("AST S-expression requires an atom head");
    if (expression.items.length == 2 && expression.items[1].is_atom)
        return ASTNode(expression.items[0].text, expression.items[1].text);
    ASTNode[] children;
    foreach (item; expression.items[1 .. $]) children ~= sexpr_to_ast(item);
    return ASTNode(expression.items[0].text, children);
}
alias EClassId = size_t;
alias ESubstitution = EClassId[string];
struct EMatch { EClassId root; ESubstitution bindings; }
struct ERewriteRule { string name; SExpr lhs, rhs; }
bool is_pattern_variable(SExpr term)
{ return term.is_atom && !term.quoted && term.text.length > 1 && term.text[0] == '?'; }
private void validateETerm(SExpr term, ref bool[string] variables)
{
    if (is_pattern_variable(term)) { variables[term.text] = true; return; }
    if (term.is_atom) return;
    if (!term.items.length || !term.items[0].is_atom || is_pattern_variable(term.items[0]))
        throw new Exception("e-graph term requires a literal operator head");
    foreach (child; term.items[1 .. $]) validateETerm(child, variables);
}
ERewriteRule sexpr_rule(SExpr expression)
{
    auto items = expression.items;
    if (expression.is_atom || !items.length || !items[0].is_atom) throw new Exception("expected rewrite rule");
    ERewriteRule rule;
    if (items[0].text == "rewrite" && items.length == 4 && items[1].is_atom)
        rule = ERewriteRule(items[1].text, items[2], items[3]);
    else if ((items[0].text == "=" || items[0].text == "=>") && items.length == 3)
        rule = ERewriteRule(items[0].text, items[1], items[2]);
    else throw new Exception("rule syntax is (rewrite name lhs rhs) or (= lhs rhs)");
    bool[string] left, right;
    validateETerm(rule.lhs, left); validateETerm(rule.rhs, right);
    foreach (variable; right.byKey) if (variable !in left) throw new Exception("unbound RHS variable " ~ variable);
    return rule;
}
ERewriteRule sexpr_rule(string source) { return sexpr_rule(parse_sexpr(source)); }
ERewriteRule[] sexpr_rules(string source)
{
    ERewriteRule[] result;
    foreach (expression; parse_sexpressions(source)) result ~= sexpr_rule(expression);
    return result;
}
struct SaturationLimits { size_t iterations = 30, nodes = 100_000, matches = 100_000; }
enum SaturationStop { Saturated, IterationLimit, NodeLimit, MatchLimit }
struct SaturationResult
{
    SaturationStop stop = SaturationStop.IterationLimit;
    size_t iterations, applications;
    bool saturated() const { return stop == SaturationStop.Saturated; }
}
class EGraph
{
    struct ENode { string op; bool atom, quoted; EClassId[] children; }
    private struct EClass { EClassId parent; ENode[] nodes; }
    private class NodeLimit : Exception { this() { super("e-graph node limit"); } }
    private EClass[] classes;
    private EClassId[string] hash;
    private bool dirty;
    private size_t revision, budget = size_t.max;
    private string key(ref ENode node)
    {
        string result = (node.atom ? "a" : "n") ~ (node.quoted ? "q" : "u") ~ to!string(node.op.length) ~ ":" ~ node.op;
        foreach (ref child; node.children) { child = find(child); result ~= ":" ~ to!string(child); }
        return result;
    }
    private EClassId insert(ENode node)
    {
        rebuild();
        auto encoded = key(node);
        if (auto found = encoded in hash) return find(*found);
        if (hash.length >= budget) throw new NodeLimit;
        auto id = classes.length;
        classes ~= EClass(id, [node]); hash[encoded] = id;
        ++revision;
        return id;
    }
    private ESubstitution[] matchTerm(SExpr term, EClassId root, ESubstitution bindings, size_t limit)
    {
        root = find(root);
        if (is_pattern_variable(term))
        {
            if (auto found = term.text in bindings) if (find(*found) != root) return null;
            auto result = bindings.dup;
            result[term.text] = root;
            return [result];
        }
        ESubstitution[] result;
        foreach (node; classes[root].nodes)
        {
            if (node.atom != term.is_atom) continue;
            auto head = term.is_atom ? term : term.items[0];
            if (node.op != head.text || node.quoted != head.quoted) continue;
            if (term.is_atom) result ~= bindings.dup;
            else if (node.children.length + 1 == term.items.length)
            {
                ESubstitution[] partial = [bindings.dup];
                foreach (i, child; node.children)
                {
                    ESubstitution[] next;
                    foreach (b; partial)
                    {
                        foreach (item; matchTerm(term.items[i + 1], child, b, limit))
                        { next ~= item; if (next.length >= limit) break; }
                        if (next.length >= limit) break;
                    }
                    partial = next;
                    if (!partial.length) break;
                }
                foreach (item; partial) { result ~= item; if (result.length >= limit) break; }
            }
            if (result.length >= limit) break;
        }
        return result;
    }
    private EClassId instantiate(SExpr term, ESubstitution bindings)
    {
        if (is_pattern_variable(term)) return find(bindings[term.text]);
        if (term.is_atom) return insert(ENode(term.text, true, term.quoted));
        ENode node = ENode(term.items[0].text, false, term.items[0].quoted);
        foreach (child; term.items[1 .. $]) node.children ~= instantiate(child, bindings);
        return insert(node);
    }
    EClassId find(EClassId id)
    {
        if (id >= classes.length) throw new Exception("invalid e-class ID");
        auto root = id;
        while (classes[root].parent != root) root = classes[root].parent;
        while (classes[id].parent != id) { auto next = classes[id].parent; classes[id].parent = root; id = next; }
        return root;
    }
    bool unite(EClassId a, EClassId b)
    {
        a = find(a); b = find(b);
        if (a == b) return false;
        if (a > b) { auto temp = a; a = b; b = temp; }
        classes[b].parent = a;
        classes[a].nodes ~= classes[b].nodes;
        classes[b].nodes = null;
        dirty = true; ++revision;
        return true;
    }
    void rebuild()
    {
        if (!dirty) return;
        struct OwnedNode { EClassId owner; ENode node; }
        OwnedNode[] nodes;
        foreach (i, cls; classes) foreach (node; cls.nodes) nodes ~= OwnedNode(i, node);
        bool changed;
        do
        {
            changed = false; hash = null;
            foreach (ref item; nodes)
            {
                auto encoded = key(item.node);
                if (auto found = encoded in hash) changed |= unite(item.owner, *found);
                else hash[encoded] = find(item.owner);
            }
        } while (changed);
        foreach (ref cls; classes) cls.nodes = null;
        hash = null;
        foreach (ref item; nodes)
        {
            auto encoded = key(item.node);
            if (encoded in hash) continue;
            auto root = find(item.owner); hash[encoded] = root;
            classes[root].nodes ~= item.node;
        }
        dirty = false;
    }
    EClassId add(SExpr expression)
    {
        bool[string] variables;
        validateETerm(expression, variables);
        if (variables.length) throw new Exception("cannot add an unbound pattern to an e-graph");
        return instantiate(expression, null);
    }
    EClassId add(string expression) { return add(parse_sexpr(expression)); }
    EClassId add(ASTNode tree) { return add(ast_to_sexpr(tree)); }
    bool equivalent(EClassId a, EClassId b) { rebuild(); return find(a) == find(b); }
    bool when_equal(F)(EClassId a, EClassId b, F rewriter)
    { if (!equivalent(a, b)) return false; rewriter(this, find(a)); return true; }
    size_t node_count() { rebuild(); return hash.length; }
    size_t class_count()
    { rebuild(); size_t count; foreach (i; 0 .. classes.length) count += find(i) == i; return count; }
    EMatch[] matches(SExpr pattern, size_t limit = 100_000)
    {
        bool[string] variables; validateETerm(pattern, variables);
        rebuild(); EMatch[] result;
        if (!limit) return result;
        foreach (i; 0 .. classes.length) if (find(i) == i)
            foreach (bindings; matchTerm(pattern, i, null, limit - result.length))
            { result ~= EMatch(i, bindings); if (result.length == limit) return result; }
        return result;
    }
    SaturationResult saturate(ERewriteRule[] rules, SaturationLimits limits = SaturationLimits.init,
        void delegate(ERewriteRule, EMatch) on_match = null)
    {
        foreach (rule; rules)
            sexpr_rule(SExpr.list([SExpr.atom("rewrite"), SExpr.atom(rule.name), rule.lhs, rule.rhs]));
        auto oldBudget = budget; budget = limits.nodes;
        scope(exit) budget = oldBudget;
        SaturationResult report;
        if (node_count() > limits.nodes) { report.stop = SaturationStop.NodeLimit; return report; }
        foreach (iteration; 0 .. limits.iterations)
        {
            rebuild(); auto before = revision;
            ++report.iterations;
            foreach (rule; rules)
            {
                auto remaining = limits.matches - report.applications;
                foreach (match; matches(rule.lhs, remaining == size_t.max ? remaining : remaining + 1))
                {
                    if (report.applications == limits.matches) { report.stop = SaturationStop.MatchLimit; return report; }
                    try { auto rhs = instantiate(rule.rhs, match.bindings); unite(match.root, rhs); }
                    catch (NodeLimit) { report.stop = SaturationStop.NodeLimit; rebuild(); return report; }
                    ++report.applications;
                    if (on_match !is null) on_match(rule, match);
                }
            }
            rebuild();
            if (revision == before) { report.stop = SaturationStop.Saturated; return report; }
        }
        return report;
    }
    SExpr extract(EClassId id)
    {
        rebuild(); id = find(id);
        auto costs = new size_t[classes.length]; costs[] = size_t.max;
        auto choices = new size_t[classes.length]; choices[] = size_t.max;
        bool changed;
        do
        {
            changed = false;
            foreach (i; 0 .. classes.length) if (find(i) == i)
                foreach (j, node; classes[i].nodes)
                {
                    size_t cost = 1;
                    foreach (child; node.children)
                    {
                        auto c = costs[find(child)];
                        if (c == size_t.max || c >= size_t.max - cost) { cost = size_t.max; break; }
                        cost += c;
                    }
                    if (cost < costs[i]) { costs[i] = cost; choices[i] = j; changed = true; }
                }
        } while (changed);
        if (choices[id] == size_t.max) throw new Exception("e-class has no finite expression");
        SExpr build(EClassId root)
        {
            root = find(root); auto node = classes[root].nodes[choices[root]];
            if (node.atom) return SExpr.atom(node.op, node.quoted);
            SExpr[] items = [SExpr.atom(node.op, node.quoted)];
            foreach (child; node.children) items ~= build(child);
            return SExpr.list(items);
        }
        return build(id);
    }
    ASTNode extract_ast(EClassId id) { return sexpr_to_ast(extract(id)); }
}

// User-defined serialization and byte I/O. Adapters borrow their backends.
template SerializerBackend(B, T, Output)
{ enum SerializerBackend = __traits(compiles, (*cast(B*) null).serialize(*cast(T*) null, *cast(Output*) null)); }
template DeserializerBackend(B, T, Input)
{ enum DeserializerBackend = __traits(compiles, (*cast(B*) null).deserialize!T(*cast(Input*) null)); }
struct Serializer(B)
{
    B* backend;
    this(ref B backend) { this.backend = &backend; }
    auto serialize(T, Output)(auto ref T value, ref Output output)
    { return backend.serialize(value, output); }
}
struct Deserializer(B)
{
    B* backend;
    this(ref B backend) { this.backend = &backend; }
    auto deserialize(T, Input)(ref Input input) { return backend.deserialize!T(input); }
}
enum IOBackend(B) = __traits(compiles, B.init.read(ubyte[].init)) &&
    __traits(compiles, B.init.write(cast(const(ubyte)[]) null));
struct IO(B) if (IOBackend!B)
{
    B* backend;
    this(ref B backend) { this.backend = &backend; }
    size_t read(ubyte[] bytes)
    {
        auto n = backend.read(bytes);
        if (n > bytes.length) throw new Exception("I/O backend returned an invalid read count");
        return n;
    }
    size_t write(const(ubyte)[] bytes)
    {
        auto n = backend.write(bytes);
        if (n > bytes.length) throw new Exception("I/O backend returned an invalid write count");
        return n;
    }
    void read_exact(ubyte[] bytes)
    {
        while (bytes.length)
        { auto n = read(bytes); if (!n) throw new Exception("unexpected I/O EOF"); bytes = bytes[n .. $]; }
    }
    void write_all(const(ubyte)[] bytes)
    {
        while (bytes.length)
        { auto n = write(bytes); if (!n) throw new Exception("I/O write made no progress"); bytes = bytes[n .. $]; }
    }
    void seek()(size_t position) if (__traits(hasMember, B, "seek")) { backend.seek(position); }
    size_t tell()() if (__traits(hasMember, B, "tell")) { return backend.tell(); }
}

// Visitors derived from ASTs. The dispatch and handler types are specialized.
struct ASTTag(string tag) { enum value = tag; }
struct VisitorCase(string tag, F)
{
    enum name = tag;
    F fn;
    void invoke(ref ASTNode node)
    {
        static if (__traits(compiles, fn(ASTTag!tag(), node))) fn(ASTTag!tag(), node);
        else fn(node);
    }
}
auto on(string tag, F)(F fn) { return VisitorCase!(tag, F)(fn); }
auto on(string tag, alias fn)() { return on!tag(StaticCallable!fn()); }
enum VisitOrder { Preorder, Postorder }
private string[] astSchemaTags(ASTNode node)
{
    string[] tags = [node.tag()];
    foreach (child; node.children())
        foreach (tag; astSchemaTags(child))
        { bool found; foreach (existing; tags) found |= existing == tag; if (!found) tags ~= tag; }
    return tags;
}
struct ASTVisitor(C...)
{
    Tuple!C cases;
    string[] tags;
    this(ASTNode schema, C cases)
    {
        this.cases = tuple(cases);
        tags = astSchemaTags(schema);
        foreach (tag; tags)
        {
            bool found;
            static foreach (Case; C) found |= tag == Case.name;
            if (!found) throw new Exception("visitor lacks a handler for " ~ tag);
        }
    }
    void visit(ref ASTNode node)
    {
        bool allowed;
        foreach (tag; tags) allowed |= tag == node.tag();
        if (!allowed) throw new Exception("node tag is outside visitor schema");
        static foreach (i; 0 .. C.length)
            if (node.tag() == C[i].name) { cases[i].invoke(node); return; }
    }
    void walk(ref ASTNode node, VisitOrder order = VisitOrder.Preorder)
    {
        if (order == VisitOrder.Preorder) visit(node);
        foreach (ref child; node.children()) walk(child, order);
        if (order == VisitOrder.Postorder) visit(node);
    }
}
auto derive_visitor(C...)(ASTNode schema, C cases) { return ASTVisitor!C(schema, cases); }
auto compile_visitor(alias schema, C...)(C cases)
{
    enum tags = astSchemaTags(schema);
    static foreach (tag; tags)
    {
        static assert(() {
            bool found; static foreach (Case; C) found |= tag == Case.name; return found;
        }(), "visitor must handle schema tag " ~ tag);
    }
    return derive_visitor(schema, cases);
}
auto compile_visitor(C...)(ASTNode schema, C cases) { return derive_visitor(schema, cases); }

// Compile-time byte regular expressions (Thompson NFA, byte rather than Unicode semantics).
struct RegexMatch { size_t begin, end; size_t size() const { return end - begin; } }
private enum ByteRegexOp { Byte, Any, Class, Split, Empty, Begin, End, Accept }
private struct ByteRegexState
{
    ByteRegexOp op = ByteRegexOp.Empty;
    int x = -1, y = -1;
    ubyte byte_value;
    bool[256] chars;
}
private struct ByteRegexProgram { ByteRegexState[] states; int start; bool valid = true; }
private struct ByteRegexFragment { int start; Tuple!(int, bool)[] ends; }
private struct ByteRegexCompiler
{
    string source;
    size_t pos;
    ByteRegexProgram program;
    int emit(ByteRegexState state) { auto index = cast(int) program.states.length; program.states ~= state; return index; }
    void patch(ByteRegexFragment fragment, int destination)
    {
        foreach (item; fragment.ends)
            if (item[1]) program.states[item[0]].y = destination;
            else program.states[item[0]].x = destination;
    }
    ByteRegexFragment one(ByteRegexState state)
    { auto index = emit(state); return ByteRegexFragment(index, [tuple(index, false)]); }
    static int hex(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }
    ubyte escaped()
    {
        if (pos == source.length) { program.valid = false; return 0; }
        char c = source[pos++];
        switch (c)
        {
            case 'n': return '\n'; case 'r': return '\r'; case 't': return '\t'; case '0': return 0;
            case 'x':
                if (pos + 2 > source.length || hex(source[pos]) < 0 || hex(source[pos + 1]) < 0)
                { program.valid = false; return 0; }
                auto value = hex(source[pos]) * 16 + hex(source[pos + 1]); pos += 2; return cast(ubyte) value;
            default: return cast(ubyte) c;
        }
    }
    ByteRegexFragment atom()
    {
        if (pos == source.length) return one(ByteRegexState(ByteRegexOp.Empty));
        char c = source[pos++];
        if (c == '(')
        {
            auto fragment = expression();
            if (pos == source.length || source[pos++] != ')') program.valid = false;
            return fragment;
        }
        ByteRegexState state;
        if (c == '[')
        {
            state.op = ByteRegexOp.Class;
            bool negate = pos < source.length && source[pos] == '^'; if (negate) ++pos;
            bool any;
            while (pos < source.length && source[pos] != ']')
            {
                ubyte first = cast(ubyte) source[pos++]; if (first == '\\') first = escaped();
                ubyte last = first;
                if (pos + 1 < source.length && source[pos] == '-' && source[pos + 1] != ']')
                { ++pos; last = cast(ubyte) source[pos++]; if (last == '\\') last = escaped(); }
                if (first > last) program.valid = false;
                for (uint value = first; value <= last; ++value) state.chars[value] = true;
                any = true;
            }
            if (!any || pos == source.length || source[pos++] != ']') program.valid = false;
            if (negate) foreach (ref value; state.chars) value = !value;
        }
        else if (c == '.') state.op = ByteRegexOp.Any;
        else if (c == '^') state.op = ByteRegexOp.Begin;
        else if (c == '$') state.op = ByteRegexOp.End;
        else
        {
            if (c == '*' || c == '+' || c == '?' || c == ')') program.valid = false;
            state.op = ByteRegexOp.Byte; state.byte_value = c == '\\' ? escaped() : cast(ubyte) c;
        }
        return one(state);
    }
    ByteRegexFragment repeated()
    {
        auto fragment = atom();
        if (pos < source.length && (source[pos] == '*' || source[pos] == '+' || source[pos] == '?'))
        {
            char quant = source[pos++]; auto split = emit(ByteRegexState(ByteRegexOp.Split, fragment.start, -1));
            if (quant == '?') { fragment.ends ~= tuple(split, true); return ByteRegexFragment(split, fragment.ends); }
            patch(fragment, split);
            return ByteRegexFragment(quant == '+' ? fragment.start : split, [tuple(split, true)]);
        }
        return fragment;
    }
    ByteRegexFragment sequence()
    {
        auto result = one(ByteRegexState(ByteRegexOp.Empty));
        while (pos < source.length && source[pos] != ')' && source[pos] != '|')
        { auto next = repeated(); patch(result, next.start); result.ends = next.ends; }
        return result;
    }
    ByteRegexFragment expression()
    {
        auto left = sequence();
        while (pos < source.length && source[pos] == '|')
        {
            ++pos; auto right = sequence(); auto split = emit(ByteRegexState(ByteRegexOp.Split, left.start, right.start));
            left.ends ~= right.ends; left.start = split;
        }
        return left;
    }
    ByteRegexProgram compile()
    {
        auto result = expression(); patch(result, emit(ByteRegexState(ByteRegexOp.Accept)));
        program.start = result.start; program.valid &= pos == source.length; return program;
    }
}
/// Supports grouping, alternation, . [] [^] ranges, * + ?, ^ $, and escaped
/// literals / \xHH / \0 / \n / \r / \t. No captures; all 256 bytes are valid.
struct StaticRegex(string source)
{
    private enum compiled = ByteRegexCompiler(source).compile();
    static assert(compiled.valid, "invalid static byte regular expression");
    private static immutable ByteRegexProgram program = compiled;
    static Nullable!size_t prefix(const(ubyte)[] bytes, size_t start = 0)
    {
        if (start > bytes.length) return Nullable!size_t.init;
        int[] active, next;
        auto seen = new bool[program.states.length];
        void closure(int index, size_t pos, ref int[] target)
        {
            if (index < 0 || seen[index]) return;
            seen[index] = true;
            auto state = program.states[index];
            if (state.op == ByteRegexOp.Split) { closure(state.x, pos, target); closure(state.y, pos, target); }
            else if (state.op == ByteRegexOp.Empty || (state.op == ByteRegexOp.Begin && pos == 0) ||
                     (state.op == ByteRegexOp.End && pos == bytes.length)) closure(state.x, pos, target);
            else if (state.op != ByteRegexOp.Begin && state.op != ByteRegexOp.End) target ~= index;
        }
        closure(program.start, start, active);
        Nullable!size_t best;
        for (auto pos = start; ; ++pos)
        {
            foreach (index; active) if (program.states[index].op == ByteRegexOp.Accept) best = pos;
            if (pos == bytes.length || !active.length) break;
            next = null; seen[] = false;
            auto value = bytes[pos];
            foreach (index; active)
            {
                auto state = program.states[index];
                if (state.op == ByteRegexOp.Any || (state.op == ByteRegexOp.Byte && state.byte_value == value) ||
                    (state.op == ByteRegexOp.Class && state.chars[value])) closure(state.x, pos + 1, next);
            }
            active = next;
        }
        return best;
    }
    static Nullable!RegexMatch search(const(ubyte)[] bytes, size_t start = 0)
    {
        for (auto i = start; i <= bytes.length; ++i)
        { auto end = prefix(bytes, i); if (!end.isNull) return Nullable!RegexMatch(RegexMatch(i, end.get)); }
        return Nullable!RegexMatch.init;
    }
    static bool matches(const(ubyte)[] bytes) { auto end = prefix(bytes); return !end.isNull && end.get == bytes.length; }
    static bool matches(string text) { return matches(cast(const(ubyte)[]) text); }
}
enum ByteOrder { Little, Big }
struct BinaryStream
{
    ubyte[] bytes;
    size_t pos;
    this(const(ubyte)[] input) { bytes = input.dup; }
    bool eof() const { return pos >= bytes.length; }
    const(ubyte)[] read(size_t count)
    {
        if (pos > bytes.length || count > bytes.length - pos) throw new Exception("binary stream EOF");
        auto view = bytes[pos .. pos + count]; pos += count; return view;
    }
    void write(const(ubyte)[] input) { bytes ~= input; }
    Nullable!(const(ubyte)[]) next(string regex)()
    {
        auto end = StaticRegex!regex.prefix(bytes, pos);
        if (end.isNull) return Nullable!(const(ubyte)[]).init;
        if (end.get == pos) throw new Exception("binary token regex matched empty input");
        return Nullable!(const(ubyte)[])(read(end.get - pos));
    }
    T read_integer(T)(ByteOrder order = ByteOrder.Little) if (isIntegral!T)
    {
        import std.traits : isUnsigned;
        static assert(isUnsigned!T, "binary integers must be unsigned");
        auto input = read(T.sizeof); T value;
        foreach (i; 0 .. T.sizeof)
        { auto shift = order == ByteOrder.Little ? i : T.sizeof - 1 - i; value |= cast(T) input[i] << (shift * 8); }
        return value;
    }
    void write_integer(T)(T value, ByteOrder order = ByteOrder.Little) if (isIntegral!T)
    {
        import std.traits : isUnsigned;
        static assert(isUnsigned!T, "binary integers must be unsigned");
        foreach (i; 0 .. T.sizeof)
        { auto shift = order == ByteOrder.Little ? i : T.sizeof - 1 - i; bytes ~= cast(ubyte) (value >> (shift * 8)); }
    }
    static BinaryStream load(B)(ref IO!B input, size_t limit = 64 * 1024 * 1024)
    {
        BinaryStream result; ubyte[4096] buffer;
        while (true)
        {
            auto n = input.read(buffer[]); if (!n) return result;
            if (n > limit - result.bytes.length) throw new Exception("binary stream input limit exceeded");
            result.write(buffer[0 .. n]);
        }
    }
    void save(B)(ref IO!B output) { output.write_all(bytes); }
}

// Explicit context-free grammar, independent of PEG ordered-choice semantics.
struct CFGSymbol
{
    bool terminal;
    string name;
    static CFGSymbol token(string text) { return CFGSymbol(true, text); }
    static CFGSymbol nonterminal(string name) { return CFGSymbol(false, name); }
}
struct CFGRule { string lhs; CFGSymbol[] rhs; }
struct CFGParseResult
{
    Nullable!ASTNode tree;
    size_t position;
    string[] expected;
    bool ok() const { return !tree.isNull; }
}
struct CFG
{
    string start;
    CFGRule[] productions;
    void add(string lhs, CFGSymbol[] rhs) { productions ~= CFGRule(lhs, rhs.dup); }
    void validate()
    {
        bool[string] names;
        foreach (rule; productions)
        { if (!rule.lhs.length) throw new Exception("empty CFG nonterminal name"); names[rule.lhs] = true; }
        if (start !in names) throw new Exception("undefined CFG start symbol");
        foreach (rule; productions) foreach (symbol; rule.rhs)
            if (!symbol.terminal && symbol.name !in names) throw new Exception("undefined CFG nonterminal " ~ symbol.name);
    }
    CFGParseResult parse(string[] tokens)
    {
        validate();
        auto rules = [CFGRule("$start", [CFGSymbol.nonterminal(start)])] ~ productions;
        struct Item { size_t rule, dot, origin; ASTNode[] children; }
        auto chart = new Item[][tokens.length + 1];
        void insert(size_t position, Item item)
        {
            foreach (old; chart[position])
                if (old.rule == item.rule && old.dot == item.dot && old.origin == item.origin) return;
            chart[position] ~= item;
        }
        insert(0, Item(0, 0, 0));
        foreach (pos; 0 .. tokens.length + 1)
        {
            for (size_t index = 0; index < chart[pos].length; ++index)
            {
                auto item = chart[pos][index]; auto rule = rules[item.rule];
                if (item.dot == rule.rhs.length)
                {
                    auto tree = ASTNode(rule.lhs, item.children);
                    for (size_t k = 0; k < chart[item.origin].length; ++k)
                    {
                        auto waiting = chart[item.origin][k]; auto parent = rules[waiting.rule];
                        if (waiting.dot < parent.rhs.length && !parent.rhs[waiting.dot].terminal && parent.rhs[waiting.dot].name == rule.lhs)
                        { ++waiting.dot; waiting.children = waiting.children ~ tree; insert(pos, waiting); }
                    }
                }
                else
                {
                    auto symbol = rule.rhs[item.dot];
                    if (symbol.terminal)
                    {
                        if (pos < tokens.length && symbol.name == tokens[pos])
                        { ++item.dot; item.children = item.children ~ ASTNode("token", tokens[pos]); insert(pos + 1, item); }
                    }
                    else
                    {
                        foreach (i; 1 .. rules.length) if (rules[i].lhs == symbol.name) insert(pos, Item(i, 0, pos));
                        for (size_t k = 0; k < chart[pos].length; ++k)
                        {
                            auto completed = chart[pos][k]; auto child = rules[completed.rule];
                            if (completed.origin == pos && completed.dot == child.rhs.length && child.lhs == symbol.name)
                            {
                                auto advanced = item; ++advanced.dot;
                                advanced.children = advanced.children ~ ASTNode(child.lhs, completed.children); insert(pos, advanced);
                            }
                        }
                    }
                }
            }
        }
        CFGParseResult result;
        foreach (item; chart[$ - 1]) if (item.rule == 0 && item.dot == 1 && item.origin == 0)
        { result.tree = item.children[0]; result.position = tokens.length; return result; }
        foreach (pos; 0 .. tokens.length + 1) if (chart[pos].length) result.position = pos;
        foreach (item; chart[result.position])
        {
            auto rule = rules[item.rule];
            if (item.dot < rule.rhs.length && rule.rhs[item.dot].terminal)
            {
                auto name = rule.rhs[item.dot].name;
                bool found; foreach (old; result.expected) found |= old == name;
                if (!found) result.expected ~= name;
            }
        }
        if (result.position < tokens.length && !result.expected.length) result.expected ~= "<end-of-input>";
        return result;
    }
}
struct CFGTerminal(string text) { static CFGSymbol symbol() { return CFGSymbol.token(text); } }
struct CFGNonterminal(string name) { static CFGSymbol symbol() { return CFGSymbol.nonterminal(name); } }
struct CFGProduction(string lhs, Symbols...)
{
    static CFGRule rule()
    { CFGSymbol[] symbols; static foreach (S; Symbols) symbols ~= S.symbol(); return CFGRule(lhs, symbols); }
}
CFG make_cfg(string start, P...)(P productions)
{
    CFGRule[] rules; static foreach (Production; P) rules ~= Production.rule();
    auto grammar = CFG(start, rules); grammar.validate(); return grammar;
}
/// Quoted symbols are terminals; unquoted are nonterminals; () is epsilon.
CFG sexpr_grammar(SExpr expression)
{
    if (expression.is_atom || !expression.items.length || expression.items[0].text != "grammar")
        throw new Exception("expected (grammar ...) S-expression");
    CFG grammar;
    foreach (part; expression.items[1 .. $])
    {
        auto items = part.items;
        if (part.is_atom || !items.length) throw new Exception("invalid grammar declaration");
        if (items[0].text == "start" && items.length == 2 && items[1].is_atom) grammar.start = items[1].text;
        else if (items[0].text == "rule" && items.length >= 3 && items[1].is_atom)
        {
            foreach (alternative; items[2 .. $])
            {
                if (alternative.is_atom) throw new Exception("CFG alternative must be a list");
                CFGSymbol[] symbols;
                foreach (symbol; alternative.items)
                { if (!symbol.is_atom) throw new Exception("CFG symbol must be an atom"); symbols ~= CFGSymbol(symbol.quoted, symbol.text); }
                grammar.add(items[1].text, symbols);
            }
        }
        else throw new Exception("invalid grammar declaration");
    }
    grammar.validate(); return grammar;
}
CFG sexpr_grammar(string source) { return sexpr_grammar(parse_sexpr(source)); }

// Built-in preprocessing. Include and script operations are user-supplied.
private bool ppIdentifierStart(char c)
{ return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
private bool ppIdentifierChar(char c) { return ppIdentifierStart(c) || (c >= '0' && c <= '9'); }
private string ppTrim(string s)
{
    while (s.length && s[0] <= ' ') s = s[1 .. $];
    while (s.length && s[$ - 1] <= ' ') s = s[0 .. $ - 1];
    return s;
}
private string ppWord(ref string s)
{
    s = ppTrim(s); size_t end;
    while (end < s.length && ppIdentifierChar(s[end])) ++end;
    auto word = s[0 .. end]; s = s[end .. $]; return word;
}
struct PreprocessorMacro { string replacement; string[] parameters; bool function_macro; }
class Preprocessor
{
    PreprocessorMacro[string] macros;
    string delegate(string) include;
    string delegate(string, string) script;
    size_t max_expansion_depth = 100, max_include_depth = 32;
    private bool in_comment;
    void define(string name, string replacement) { macros[name] = PreprocessorMacro(replacement); }
    void define_function(string name, string[] parameters, string replacement)
    { macros[name] = PreprocessorMacro(replacement, parameters.dup, true); }
    private string substitute(string source, string[string] values)
    {
        string result;
        for (size_t i = 0; i < source.length;)
        {
            if (source[i] == '"' || source[i] == '\'')
            {
                char quote = source[i]; result ~= source[i++];
                while (i < source.length)
                { char c = source[i++]; result ~= c; if (c == '\\' && i < source.length) result ~= source[i++]; else if (c == quote) break; }
            }
            else if (ppIdentifierStart(source[i]))
            {
                auto begin = i++; while (i < source.length && ppIdentifierChar(source[i])) ++i;
                auto name = source[begin .. i];
                if (auto found = name in values) result ~= *found; else result ~= name;
            }
            else result ~= source[i++];
        }
        return result;
    }
    private string expandImpl(string source, bool[string] disabled, size_t depth)
    {
        if (depth > max_expansion_depth) throw new Exception("macro expansion depth exceeded");
        string result;
        for (size_t i = 0; i < source.length;)
        {
            if (in_comment)
            {
                size_t end = i;
                while (end + 1 < source.length && source[end .. end + 2] != "*/") ++end;
                if (end + 1 >= source.length) { result ~= source[i .. $]; break; }
                result ~= source[i .. end + 2]; i = end + 2; in_comment = false;
            }
            else if (i + 1 < source.length && source[i .. i + 2] == "//") { result ~= source[i .. $]; break; }
            else if (i + 1 < source.length && source[i .. i + 2] == "/*") { in_comment = true; result ~= "/*"; i += 2; }
            else if (source[i] == '"' || source[i] == '\'')
            {
                char quote = source[i]; result ~= source[i++];
                while (i < source.length)
                { char c = source[i++]; result ~= c; if (c == '\\' && i < source.length) result ~= source[i++]; else if (c == quote) break; }
            }
            else if (ppIdentifierStart(source[i]))
            {
                auto begin = i++; while (i < source.length && ppIdentifierChar(source[i])) ++i;
                auto name = source[begin .. i]; auto found = name in macros;
                if (found is null || name in disabled) { result ~= name; continue; }
                auto definition = *found; auto replacement = definition.replacement;
                if (definition.function_macro)
                {
                    auto cursor = i; while (cursor < source.length && source[cursor] <= ' ') ++cursor;
                    if (cursor == source.length || source[cursor] != '(') { result ~= name; continue; }
                    ++cursor; auto argBegin = cursor; size_t nesting = 1; string[] arguments;
                    while (cursor < source.length && nesting)
                    {
                        char c = source[cursor++];
                        if (c == '"' || c == '\'')
                        {
                            while (cursor < source.length)
                            { char q = source[cursor++]; if (q == '\\' && cursor < source.length) ++cursor; else if (q == c) break; }
                        }
                        else if (c == '(') ++nesting;
                        else if (c == ')') { if (!--nesting) arguments ~= ppTrim(source[argBegin .. cursor - 1]); }
                        else if (c == ',' && nesting == 1) { arguments ~= ppTrim(source[argBegin .. cursor - 1]); argBegin = cursor; }
                    }
                    if (nesting) throw new Exception("unterminated macro invocation");
                    if (!definition.parameters.length && arguments.length == 1 && !arguments[0].length) arguments = null;
                    if (arguments.length != definition.parameters.length) throw new Exception("macro argument count mismatch for " ~ name);
                    string[string] parameters;
                    foreach (a; 0 .. arguments.length) parameters[definition.parameters[a]] = expandImpl(arguments[a], disabled, depth + 1);
                    replacement = substitute(replacement, parameters); i = cursor;
                }
                auto next = disabled.dup; next[name] = true;
                result ~= expandImpl(replacement, next, depth + 1);
            }
            else result ~= source[i++];
        }
        return result;
    }
    private struct Condition
    {
        string source;
        size_t pos;
        Preprocessor owner;
        bool[string] disabled;
        void skip() { while (pos < source.length && source[pos] <= ' ') ++pos; }
        bool take(string text)
        { skip(); if (text.length > source.length - pos || source[pos .. pos + text.length] != text) return false; pos += text.length; return true; }
        long primary()
        {
            skip();
            if (take("!")) return !primary();
            if (take("(")) { auto value = either(); if (!take(")")) throw new Exception("missing ')' in #if"); return value; }
            auto rest = source[pos .. $]; auto name = ppWord(rest);
            if (name.length && ppIdentifierStart(name[0]))
            {
                pos = source.length - rest.length;
                if (name == "defined")
                {
                    auto paren = take("("); rest = source[pos .. $]; auto id = ppWord(rest); pos = source.length - rest.length;
                    if (!id.length || (paren && !take(")"))) throw new Exception("invalid defined expression");
                    return (id in owner.macros) !is null;
                }
                auto found = name in owner.macros;
                if (found is null || found.function_macro || name in disabled) return 0;
                auto next = disabled.dup; next[name] = true;
                return Condition(found.replacement, 0, owner, next).evaluate();
            }
            auto begin = pos;
            bool negative;
            if (pos < source.length && (source[pos] == '-' || source[pos] == '+')) { negative = source[pos] == '-'; ++pos; }
            uint radix = 10;
            if (pos + 1 < source.length && source[pos] == '0' && (source[pos + 1] == 'x' || source[pos + 1] == 'X')) { radix = 16; pos += 2; }
            ulong value; size_t digits;
            while (pos < source.length && ppIdentifierChar(source[pos]))
            {
                int digit = ByteRegexCompiler.hex(source[pos++]);
                if (digit < 0 || digit >= radix || value > (ulong.max - digit) / radix) throw new Exception("invalid #if integer");
                value = value * radix + digit; ++digits;
            }
            if (!digits || begin == pos || value > cast(ulong) long.max + (negative ? 1UL : 0UL)) throw new Exception("invalid #if integer");
            return negative ? -cast(long) value : cast(long) value;
        }
        long compare()
        {
            auto a = primary();
            if (take("==")) return a == primary(); if (take("!=")) return a != primary();
            if (take("<=")) return a <= primary(); if (take(">=")) return a >= primary();
            if (take("<")) return a < primary(); if (take(">")) return a > primary();
            return a;
        }
        long both() { auto a = compare(); while (take("&&")) { auto b = compare(); a = a && b; } return a; }
        long either() { auto a = both(); while (take("||")) { auto b = both(); a = a || b; } return a; }
        long evaluate() { auto value = either(); skip(); if (pos != source.length) throw new Exception("unsupported #if expression"); return value; }
    }
    string expand(string source) { return expandImpl(source, null, 0); }
    /// Object/function macros, includes, conditionals and optional scripts.
    /// Conditions: integers, defined, !, comparisons, &&, ||. No #/## operators.
    string process(string source, size_t depth = 0)
    {
        if (depth > max_include_depth) throw new Exception("preprocessor include depth exceeded");
        string joined;
        foreach (i; 0 .. source.length)
        {
            if (i && source[i] == '\n' && source[i - 1] == '\\') continue;
            if (source[i] == '\\' && i + 1 < source.length && source[i + 1] == '\n') continue;
            joined ~= source[i];
        }
        source = joined;
        struct Branch { bool parent, taken, active, seen_else; }
        Branch[] branches;
        string output;
        bool active() { return !branches.length || branches[$ - 1].active; }
        for (size_t begin = 0; begin < source.length;)
        {
            size_t end = begin; while (end < source.length && source[end] != '\n') ++end;
            bool newline = end < source.length;
            auto line = source[begin .. end]; begin = end + (newline ? 1 : 0);
            auto directive = ppTrim(line);
            if (in_comment || !directive.length || directive[0] != '#')
            {
                if (active()) { output ~= expand(line); if (newline) output ~= "\n"; }
                else { bool[string] disabled; foreach (name; macros.byKey) disabled[name] = true; expandImpl(line, disabled, 0); }
                continue;
            }
            directive = directive[1 .. $]; auto command = ppWord(directive); directive = ppTrim(directive);
            if (command == "if" || command == "ifdef" || command == "ifndef")
            {
                bool parent = active(), value;
                if (parent)
                {
                    if (command == "if") value = Condition(directive, 0, this).evaluate() != 0;
                    else { value = (directive in macros) !is null; if (command == "ifndef") value = !value; }
                }
                branches ~= Branch(parent, value, parent && value);
            }
            else if (command == "else" || command == "elif")
            {
                if (!branches.length || branches[$ - 1].seen_else) throw new Exception("unmatched or duplicate conditional branch");
                auto branch = &branches[$ - 1];
                bool value = command == "else" || (branch.parent && !branch.taken && Condition(directive, 0, this).evaluate() != 0);
                branch.active = branch.parent && !branch.taken && value; branch.taken |= value; branch.seen_else = command == "else";
            }
            else if (command == "endif")
            { if (!branches.length) throw new Exception("unmatched #endif"); branches.length -= 1; }
            else if (!active()) continue;
            else if (command == "define")
            {
                auto name = ppWord(directive);
                if (!name.length || !ppIdentifierStart(name[0])) throw new Exception("invalid macro name");
                if (directive.length && directive[0] == '(')
                {
                    size_t close = 1; while (close < directive.length && directive[close] != ')') ++close;
                    if (close == directive.length) throw new Exception("unterminated macro parameters");
                    auto params = directive[1 .. close]; string[] parameters;
                    while (ppTrim(params).length)
                    {
                        size_t comma; while (comma < params.length && params[comma] != ',') ++comma;
                        auto param = ppTrim(params[0 .. comma]); auto check = param; auto id = ppWord(check);
                        bool duplicate; foreach (old; parameters) duplicate |= old == id;
                        if (!id.length || !ppIdentifierStart(id[0]) || ppTrim(check).length || duplicate) throw new Exception("invalid macro parameter");
                        parameters ~= id; if (comma == params.length) break; params = params[comma + 1 .. $];
                        if (!ppTrim(params).length) throw new Exception("trailing comma in macro parameters");
                    }
                    define_function(name, parameters, ppTrim(directive[close + 1 .. $]));
                }
                else define(name, ppTrim(directive));
            }
            else if (command == "undef") macros.remove(directive);
            else if (command == "include")
            {
                auto path = ppTrim(expand(directive));
                if (path.length < 2 || !((path[0] == '"' && path[$ - 1] == '"') || (path[0] == '<' && path[$ - 1] == '>')) || include is null)
                    throw new Exception("#include requires a resolver and a quoted or angle-bracket path");
                output ~= process(include(path[1 .. $ - 1]), depth + 1);
                if (newline && output.length && output[$ - 1] != '\n') output ~= "\n";
            }
            else if (command == "lua" || command == "script")
            {
                if (script is null) throw new Exception("script directive requires a user script backend");
                auto language = command == "lua" ? "lua" : ppWord(directive);
                output ~= expand(script(language, ppTrim(directive))); if (newline) output ~= "\n";
            }
            else if (command == "error") throw new Exception(directive);
            else throw new Exception("unknown preprocessor directive " ~ command);
        }
        if (branches.length) throw new Exception("unterminated preprocessor conditional");
        return output;
    }
}
version (DSLtkLua)
{
    // Lua 5.3/5.4 ABI. The user owns the state and links the Lua library.
    struct lua_State;
    extern(C)
    {
        lua_State* luaL_newstate();
        void lua_close(lua_State*);
        void luaL_openlibs(lua_State*);
        int lua_gettop(lua_State*);
        void lua_settop(lua_State*, int);
        int luaL_loadbufferx(lua_State*, const(char)*, size_t, const(char)*, const(char)*);
        int lua_pcallk(lua_State*, int, int, int, ptrdiff_t, int function(lua_State*, int, ptrdiff_t));
        const(char)* lua_tolstring(lua_State*, int, size_t*);
    }
    auto lua_script_hook(lua_State* state)
    {
        return (string language, string code) {
            import std.string : fromStringz;
            if (state is null || language != "lua") throw new Exception("invalid Lua script backend");
            auto top = lua_gettop(state); scope(exit) lua_settop(state, top);
            if (luaL_loadbufferx(state, code.ptr, code.length, "DSLtk preprocessor".ptr, null) || lua_pcallk(state, 0, 1, 0, 0, null))
                throw new Exception(fromStringz(lua_tolstring(state, -1, null)).idup);
            size_t length; auto value = lua_tolstring(state, -1, &length);
            if (value is null) throw new Exception("Lua preprocessing must return a string");
            return value[0 .. length].idup;
        };
    }
    unittest
    {
        auto state = luaL_newstate();
        assert(state !is null); scope(exit) lua_close(state);
        auto pp = new Preprocessor;
        pp.script = lua_script_hook(state);
        assert(pp.process("#lua return '(= a b)'\n") == "(= a b)\n");
        assert(lua_gettop(state) == 0);
        bool failed;
        try { pp.process("#lua !invalid!\n"); } catch (Exception) { failed = true; }
        assert(failed && lua_gettop(state) == 0);
    }
}

// Integration and regression tests instantiate the public template APIs.
unittest
{
    auto expression = parse_sexpr(";comment\n(add \"hello world\" (num 2))");
    assert(parse_sexpr(expression.dump()) == expression);
    assert(parse_sexpr("\"a\\n\\\"b\"").text == "a\n\"b");
    assert(parse_sexpr("café").text == "café");
    assert(parse_sexpr(SExpr.atom("hello world").dump()) == SExpr.atom("hello world"));
    bool invalid;
    try { sexpr_rule("(= ?x ?y)"); } catch (Exception) { invalid = true; }
    assert(invalid);
    auto tree = node!"add"(leaf!"num"(1), leaf!"num"(2));
    assert(sexpr_to_ast(ast_to_sexpr(tree)) == tree);
    auto graph = new EGraph;
    auto root = graph.add("(mul (add a 0) 1)"); auto a = graph.add("a");
    size_t observed;
    auto report = graph.saturate(sexpr_rules("(rewrite add-zero (add ?x 0) ?x) (= (mul ?x 1) ?x)"), SaturationLimits.init,
        (ERewriteRule r, EMatch m) { ++observed; });
    assert(report.saturated() && observed && graph.equivalent(root, a));
    assert(graph.extract(root).dump() == "a");
    bool triggered;
    assert(graph.when_equal(root, a, (EGraph g, EClassId id) { triggered = true; }) && triggered);
    auto congruence = new EGraph;
    auto x = congruence.add("a"), y = congruence.add("b");
    auto fx = congruence.add("(f a)"), fy = congruence.add("(f b)");
    assert(!congruence.equivalent(fx, fy));
    congruence.unite(x, y);
    assert(congruence.equivalent(fx, fy) && congruence.node_count() == 3);
    congruence.add("(pair a b)");
    assert(congruence.matches(parse_sexpr("(pair ?x ?x)")).length == 1);
    auto cyclic = new EGraph; auto atom = cyclic.add("a");
    assert(cyclic.saturate(sexpr_rules("(= ?x (wrap ?x))")).saturated());
    assert(cyclic.extract(atom).text == "a");
    auto bounded = new EGraph; bounded.add("a");
    assert(bounded.saturate(sexpr_rules("(= a (f b c))"), SaturationLimits(10, 2, 100)).stop == SaturationStop.NodeLimit);
    assert(bounded.node_count() <= 2);
    assert(bounded.saturate(sexpr_rules("(= a b)"), SaturationLimits(10, 100, 0)).stop == SaturationStop.MatchLimit);
}

unittest
{
    assert(StaticRegex!"^(ab|a)+[0-9]?$".matches("aba7"));
    assert(!StaticRegex!"^(ab|a)+[0-9]?$".matches("aba77"));
    assert(StaticRegex!"a*ab".matches("aaab"));
    assert(StaticRegex!"(a?)*".matches(""));
    static assert(!__traits(compiles, StaticRegex!"[".matches("")));
    ubyte[] binary = [0, 255, 65, 66];
    assert(StaticRegex!`\x00[\x80-\xff]AB`.matches(binary));
    auto match = StaticRegex!"AB".search(binary);
    assert(match.get.begin == 2 && match.get.end == 4);
    auto stream = BinaryStream(binary);
    assert(stream.next!`\0[\x80-\xff]`().get.length == 2 && stream.pos == 2);
    assert(stream.next!"X"().isNull && stream.pos == 2);
    bool empty;
    try { stream.next!"a*"(); } catch (Exception) { empty = true; }
    assert(empty);
    BinaryStream numbers;
    numbers.write_integer(0x01020304u, ByteOrder.Big);
    assert(numbers.read_integer!uint(ByteOrder.Big) == 0x01020304u);
    struct MemoryBackend
    {
        ubyte[] data;
        size_t pos;
        size_t read(ubyte[] target)
        {
            import std.algorithm.comparison : min;
            auto n = min(target.length, data.length - pos, 2UL);
            target[0 .. n] = data[pos .. pos + n]; pos += n; return n;
        }
        size_t write(const(ubyte)[] source)
        { auto n = source.length < 2 ? source.length : 2; data ~= source[0 .. n]; return n; }
        void seek(size_t p) { pos = p; }
        size_t tell() { return pos; }
    }
    MemoryBackend backend;
    auto io = IO!MemoryBackend(backend);
    BinaryStream(binary).save(io);
    assert(backend.data == binary);
    io.seek(0);
    assert(BinaryStream.load(io).bytes == binary && io.tell() == 4);
    io.seek(0); ubyte[4] buffer; io.read_exact(buffer[]); assert(buffer[] == binary);
    struct SequentialBackend
    {
        size_t read(ubyte[] target) { return 0; }
        size_t write(const(ubyte)[] input) { return input.length; }
    }
    SequentialBackend sequential;
    auto sequentialIO = IO!SequentialBackend(sequential);
    assert(BinaryStream.load(sequentialIO).eof());
    struct UserCodec
    {
        void serialize(int value, ref BinaryStream output) { output.write_integer(cast(uint) value); }
        T deserialize(T)(ref BinaryStream input) { return cast(T) input.read_integer!uint(); }
    }
    UserCodec codec;
    static assert(SerializerBackend!(UserCodec, int, BinaryStream));
    static assert(DeserializerBackend!(UserCodec, int, BinaryStream));
    auto serializer = Serializer!UserCodec(codec); auto deserializer = Deserializer!UserCodec(codec);
    BinaryStream encoded;
    serializer.serialize(42, encoded);
    assert(deserializer.deserialize!int(encoded) == 42);
    enum schema = node!"add"(leaf!"num"(0), leaf!"num"(0));
    int sum, additions;
    auto visitor = compile_visitor!schema(
        on!"add"((ref ASTNode n) { ++additions; }),
        on!"num"((ref ASTNode n) { sum += to!int(n.value()); }));
    auto tree = node!"add"(leaf!"num"(2), leaf!"num"(3));
    visitor.walk(tree, VisitOrder.Postorder);
    assert(sum == 5 && additions == 1);
}

unittest
{
    auto grammar = sexpr_grammar(`(grammar (start Expr) (rule Expr (Expr "+" Term) (Term)) (rule Term ("n")))`);
    auto parsed = grammar.parse(["n", "+", "n", "+", "n"]);
    assert(parsed.ok() && parsed.tree.get.tag() == "Expr");
    auto failure = grammar.parse(["n", "+"]);
    assert(!failure.ok() && failure.position == 2 && failure.expected == ["n"]);
    auto nullable = sexpr_grammar(`(grammar (start S) (rule S (A A)) (rule A () ("a")))`);
    assert(nullable.parse(null).ok() && nullable.parse(["a"]).ok() && nullable.parse(["a", "a"]).ok());
    assert(!nullable.parse(["a", "a", "a"]).ok());
    auto typed = make_cfg!"S"(CFGProduction!("S", CFGTerminal!"a")());
    assert(typed.parse(["a"]).ok());
    auto pp = new Preprocessor;
    pp.include = (string path) { assert(path == "rules"); return "#define ZERO 0\n"; };
    pp.script = (string language, string code)
    { assert(language == "lua" && code == "emit_rule"); return "(= (add ?x ZERO) ?x)"; };
    auto source = pp.process("#include \"rules\"\n#define ENABLED 1\n#define ID(x) x\n" ~
        "#if defined(ENABLED) && ENABLED == 1\nID((= (mul ?x 1) ?x))\n#else\n#error inactive\n#endif\n#lua emit_rule\n");
    assert(sexpr_rules(source).length == 2);
    assert(pp.expand("\"ZERO\" ZERO /* ZERO */") == "\"ZERO\" 0 /* ZERO */");
    pp.define("SELF", "SELF"); assert(pp.expand("SELF") == "SELF");
    assert(pp.process("#if 0\nwrong\n#elif 1\nright\n#else\nwrong\n#endif\n") == "right\n");
    assert(pp.process("#if 0\n/*\n#else\n*/\n#else\nright\n#endif\n") == "right\n");
    assert(pp.process("#define X a\\\nb\nX\n") == "ab\n");
    assert(pp.process("#if 08 == 8 && 0x10 == 16\nright\n#endif\n") == "right\n");
    bool unfinished;
    try { pp.process("#if 1\n"); } catch (Exception) { unfinished = true; }
    assert(unfinished);
}

unittest
{
    struct CustomFeature
    {
        mixin template Mixin() { int custom_value() const { return 17; } }
    }
    struct Extended { mixin DSL!(Pipeline, CustomFeature); }
    static assert(HasFeature!(Extended, CustomFeature));
    assert(Extended().custom_value() == 17);
    struct Toolkit { mixin DSL!(Pipeline, Operators, AST, PatternMatch); }
    static assert(Pipeable!Toolkit && HasFeature!(Toolkit, AST));
    static assert(!HasFeature!(int, Pipeline));
    assert((Toolkit.wrap(5) | pipe((int x) => x * 2) | pipe((int x) => x + 1)) == 11);
    int calls;
    auto positive = Toolkit.make_pred((int x) => x > 0);
    auto even = predicate((int x) { ++calls; return x % 2 == 0; });
    auto both = positive & even;
    assert(!both(-2) && calls == 0);
    assert(both(4) && calls == 1);
    auto either = positive | even;
    assert(either(3) && calls == 1);
    assert((~positive)(-1));
    auto generic = predicate!((x) => x > 0)();
    assert(generic(2) && generic.negated()(-2));
    assert((4 | pipe!((x) => x + x)()) == 8);
    static assert(pattern!"^[a-z]+z$".matches("az"));
    static assert(pattern!"[0-9]+".matches("120"));
    static assert(!pattern!"[0-9]+".matches(""));
    auto dispatch = match(when!1((int k) => 7), otherwise((int k) => 9L));
    assert(dispatch(1) == 7 && dispatch(2) == 9);
    auto required = match(when!1((int key) => 1));
    bool missingThrown;
    try { required(2); } catch (Exception) { missingThrown = true; }
    assert(missingThrown);
    auto classify = match(when!(pattern!"[0-9]+")((string s) => "number"),
                          otherwise((string s) => "other"));
    assert(classify("123") == "number" && classify("a") == "other");
    auto tree = Toolkit.make_node!"add"(Toolkit.make_leaf!"num"(1), leaf!"num"(2));
    assert(tree.dump() == "(add (num 1) (num 2))");
    auto copy = tree;
    copy.child(0) = leaf!"num"(10);
    assert(tree.child(0).value() == "1" && copy.child(0).value() == "10");
    auto deep = node!"nested"(tree);
    auto deepCopy = deep;
    deepCopy.child(0).child(0) = leaf!"num"(20);
    assert(deep.child(0).child(0).value() == "1");
    auto rules = rewrite_set(rule!"fold-add"(
        (ASTNode n) => n.tag() == "add" && n.size() == 2 && n.child(0).is_leaf() && n.child(1).is_leaf(),
        (ASTNode n) => leaf!"num"(to!int(n.child(0).value()) + to!int(n.child(1).value()))));
    assert(rules.apply(node!"add"(tree, leaf!"num"(3))).dump() == "(num 6)");
    assert(tree.dump() == "(add (num 1) (num 2))");
}

unittest
{
    struct Scalar
    {
        mixin DSL!ExprTemplates;
        double val;
        double expr_value() const { return val; }
    }
    static assert(ExprLike!Scalar);
    Scalar a = Scalar(3), b = Scalar(4);
    assert((a + b * a).eval() == 15);
    assert((2 + a * b - -a).eval() == 17);
    assert((-(a + b) / 2).eval() == -3.5);
    auto x = terminal([1, 2, 3]);
    auto y = terminal([4, 5, 6]);
    auto z = terminal([7, 8, 9]);
    assert((x + y * z).eval() == [29, 42, 57]);
    assert((2 * x + -y).eval() == [-2, -1, 0]);
    bool mismatchThrown;
    try { (x + terminal([1, 2])).eval(); } catch (Exception) { mismatchThrown = true; }
    assert(mismatchThrown);
    auto units = literal_set(lit!"_km"((real n) => n * 1000), lit!"_m"((real n) => n));
    assert(units.parse("3.5_km") == 3500);
    enum distance = literal_set(lit!"_km"((real n) => n * 1000)).parse("3.5_km");
    static assert(distance == 3500);
    int calls;
    auto square = memoize!(int, int)((int n) { ++calls; return n * n; });
    assert(square(5) == 25 && square(5) == 25 && calls == 1);
    square.clear_cache();
    assert(square(5) == 25 && calls == 2);
    auto deferred = make_lazy(() { ++calls; return 42; });
    assert(!deferred.is_computed());
    assert(deferred.get() == 42 && deferred.force() == 42 && calls == 3);
    deferred.get() = 43;
    assert(deferred.force() == 43 && calls == 3);
    auto cfg = Lazy!string(() => "ready");
    assert(cfg.get() == "ready");
}

unittest
{
    assert(Maybe!int(5).map((int n) => n + 1).filter((int n) => n > 0).or_else(0) == 6);
    assert(Maybe!int.init.map((int n) => n + 1).or_else(0) == 0);
    assert(Maybe!int(5).flat_map((int n) => Maybe!string(to!string(n))).or_else("") == "5");
    auto opt = Nullable!int(3);
    assert(map(opt, (int n) => n * 2).get == 6);
    assert(filter(opt, (int n) => n == 0).isNull);
    assert(or_else(Nullable!int.init, 4) == 4);
    alias R = Result!(string, string);
    auto ok = R.from_ok("value");
    auto err = R.from_err("error");
    assert(ok.is_ok() && err.is_err());
    assert(ok.map((string s) => s.length).unwrap() == 5);
    assert(err.map((string s) => s.length).unwrap_err() == "error");
    assert(ok.and_then((string s) => R.from_ok(s ~ "!")).unwrap() == "value!");
    assert(err.and_then((string s) => R.from_ok(s)).unwrap_err() == "error");
    assert(err.map_err((string s) => s.length).unwrap_err() == 5);
    assert(Result!(int, string)(Ok(4)).unwrap() == 4);
    assert(Result!(int, string)(Err("bad")).unwrap_or(7) == 7);
}

unittest
{
    auto pair = ch('a') & ch('b');
    auto result = run_parser(pair, "ab");
    assert(!result.value.isNull && result.value.get == tuple('a', 'b'));
    auto committed = run_parser(pair | (ch('a') & ch('c')), "ac");
    assert(committed.value.isNull && committed.error.kind == ParseFailureKind.Committed);
    auto backtrack = run_parser(try_parse(pair) | (ch('a') & ch('c')), "ac");
    assert(!backtrack.value.isNull);
    auto failed = run_parser(ch('a') | ch('b'), "z");
    assert(failed.error.pos == 0 && failed.error.expected == ["b", "a"]);
    auto digits = satisfy((char c) => c >= '0' && c <= '9', "digit").many();
    assert(run_parser(digits, "123").value.get == ['1', '2', '3']);
    assert(!run_parser(ch('a').optional(), "").value.isNull);
    assert(run_parser(ch('a').optional(), "").value.get.isNull);
    assert(run_parser(ch('a'), "ab").error.expected == ["<end-of-input>"]);
    // Empty-success parsers must not make repetition loop indefinitely.
    auto empty = parser((ref ParsecInput input) => ExpectedResult!bool(true));
    auto noProgress = run_parser(empty.many(), "");
    assert(noProgress.value.isNull && noProgress.error.expected == ["<non-empty repetition>"]);
    auto peg = peg_seq(peg_and(ch('a')), ch('a'), peg_not(ch('b')));
    assert(run_parser(peg, "a").value.get.value == "a");
    assert(!run_parser(peg_choice(pair, ch('a') & ch('c')), "ac").value.isNull);
    assert(run_parser(peg_many(ch('a')), "aaa").value.get.length == 3);
    assert(run_parser(peg_many1(ch('a')), "aa").value.get[1].length == 1);
    assert(run_parser(peg_opt(ch('a')), "").value.get.isNull);
    assert(run_parser(peg_channel("@TEST", ch('a')), "a").value.get.channel.name == "@TEST");
    ParsecInput input = ParsecInput("ab");
    string[string] semantics = ["id": "pair"];
    auto action = parse_with_action(pair, input, semantics,
        (string[string] s, ParserStage stage) => s["id"] ~ ":" ~ stage.span);
    assert(action.get == "pair:ab");
    auto handler = production!"pair"((string s) => s.length);
    assert(handler.identifier == "pair" && handler.fn("ab") == 2);
}

unittest
{
    alias R = Result!(string, string);
    auto first = Task("first", (ref TaskState s) => R.from_ok("ok"));
    auto fail = Task("fail", (ref TaskState s) => R.from_err("bad"));
    auto last = Task("last", (ref TaskState s) => R.from_ok("last"));
    auto chain = first | fail | last;
    TaskState state;
    run(state, chain);
    assert(state.results.length == 2 && state.results["fail"].is_err());
    chain.policy = TaskPolicy.ContinueOnError;
    run(state, chain);
    assert(state.results.length == 3);
    upon(state, () => true, last);
    assert(state.results["last"].unwrap() == "last");
    auto stop = Task("stop", (ref TaskState s) { suspend(s); return R.from_ok("stopped"); });
    TaskState suspended;
    run(suspended, stop | last);
    assert(suspended.suspended && suspended.results.length == 1);
}

unittest
{
    auto grammar = create_peg_definition();
    static assert(!__traits(compiles, grammar.add_rule!"["()));
    string[] tokens;
    auto whitespace = grammar.add_rule!"[ \t\n]+"();
    whitespace.channel = PEGIgnoreChannel;
    auto number = grammar.add_rule!"[0-9]+"((ref PEGMatch m) { tokens ~= m.value; });
    auto ident = grammar.add_rule!"[a-z]+"((ref PEGMatch m) { tokens ~= m.value; });
    auto original = number;
    foreach (i; 0 .. 100) grammar.add_rule!"!"();
    assert(original is grammar.rules()[1]);
    assert(grammar.parse("one 12\ntwo").ok());
    assert(tokens == ["one", "12", "two"]);
    auto failure = grammar.parse("one\n?");
    assert(failure.failed() && failure.offset == 4 && failure.line == 2 && failure.column == 1);
    auto child = derive_peg_definition(grammar);
    child.add_rule!"[A-Z]+"();
    assert(child.parse("ABC").ok() && grammar.parse("ABC").failed());
    auto nullableGrammar = create_peg_definition();
    nullableGrammar.add_rule!"a*"();
    assert(nullableGrammar.parse("b").failed());
    auto negative = create_peg_definition();
    auto forbidden = negative.add_rule!"a"();
    forbidden.must_fail = true;
    assert(negative.parse("b").ok() && negative.parse("a").failed());
    size_t negativeActions;
    forbidden.semantic_action = (ref PEGMatch m) { ++negativeActions; };
    negative.parse("b");
    assert(negativeActions == 0);
    forbidden.fire_on_neg = true;
    negative.parse("b");
    assert(negativeActions == 1);
    assert(run_parser(peg_rule(number), "123").value.get.rule is number);
    auto matcher = peg_new_matcher(grammar, "one 12 ?");
    string[] matched;
    auto cb = (ref PEGMatch m) { matched ~= m.value; };
    while (!matcher.is_spent()) matcher << ident(cb) << number(cb) << matcher.wildcard(cb);
    matcher.close();
    assert(matched == ["one", "12", "?"]);
    number.channel = new_peg_channel("@NUMBER");
    auto filtered = peg_new_matcher(grammar, "12");
    filtered.exclude("@NUMBER");
    matched = null;
    filtered << filtered.on_channel(number.channel, cb) << filtered.wildcard(cb);
    assert(matched == ["1"] && filtered.position() == 1);
    auto zeroMatcher = peg_new_matcher(nullableGrammar, "b");
    bool zeroThrown;
    try { zeroMatcher << nullableGrammar.rules()[0](cb); }
    catch (Exception) { zeroThrown = true; }
    assert(zeroThrown);
    assert(CompiledPattern.compile("[^a]+b?").match_at("xxb", 0).get == 3);
}

unittest
{
    enum Color { Red = -1000, Green = 0, AliasGreen = 0, Blue = 1000 }
    static assert(enum_count!Color() == 3);
    static assert(enum_name!(Color.Red)() == "Red");
    static assert(enum_name(Color.Green) == "Green");
    static assert(enum_values!Color() == [Color.Red, Color.Green, Color.Blue]);
    static assert(enum_cast!Color("blue", case_insensitive).get == Color.Blue);
    assert(enum_cast!Color(-1000).get == Color.Red && enum_cast!Color(ulong.max).isNull);
    assert(enum_type_name!Color() == "Color" && enum_integer(Color.Blue) == 1000);
    assert(enum_entries!Color()[1].name == "Green");
    assert(enum_index(Color.Blue).get == 2 && enum_value!(Color, 0)() == Color.Red);
    assert(enum_next_value(Color.Red).get == Color.Green && enum_prev_value(Color.Red).isNull);
    assert(enum_next_value_circular(Color.Blue).get == Color.Red);
    assert(enum_prev_value_circular(Color.Red).get == Color.Blue);
    auto name = enum_switch!((c) => enum_name!(typeof(c).value)())(Color.Blue);
    assert(name == "Blue");
    assert(enum_switch!((c) => enum_name!(typeof(c).value)())(cast(Color) 4, "unknown") == "unknown");
    size_t visited;
    struct CountEnums
    {
        size_t* count;
        void opCall(T)(T constant) { ++*count; }
    }
    enum_for_each!Color(CountEnums(&visited));
    assert(visited == 3 && enum_fuse(Color.Green, Color.Blue).get == 5);
    @(EnumOptions(true)) enum Access { Read = 1, Write = 2, Execute = 4, All = 7 }
    static assert(is_flags!Access && enum_count!Access() == 3);
    assert(enum_flags_name(Access.Read | Access.Write) == "Read|Write");
    assert(enum_flags_cast!Access("read|WRITE", case_insensitive).get == cast(Access) 3);
    assert(enum_flags_cast!Access(8).isNull);
    assert(enum_flags_test(Access.All, Access.Read) && enum_flags_test_any(Access.Read, Access.All));
    @(EnumOptions(false, 2)) @(EnumNames(["c_Red": "rouge"])) enum Named { c_Red, c_Blue }
    static assert(enum_name(Named.c_Red) == "rouge" && enum_name(Named.c_Blue) == "Blue");
    EnumArray!(Color, int) counts;
    counts[Color.Blue] = 42;
    assert(counts[Color.Blue] == 42 && counts.length() == 3);
    EnumBitset!Color bits;
    bits.set(Color.Red);
    assert(bits.count() == 1 && (~bits).count() == 2);
    EnumSet!Color set;
    assert(set.insert(Color.Blue) && !set.insert(Color.Blue));
    assert(set.contains(Color.Blue) && set.size() == 1);
    foreach (color; set) assert(color == Color.Blue);
    assert(set.erase(Color.Blue) && set.empty());
}
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        
