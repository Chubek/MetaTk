#pragma once
#include "MetaML.hpp"
// Included after MetaML's runtime definitions. No call to eval_tree is made by
// this compiler or its VM. Complex values use the same GC/environment ABI.
namespace metaml {
enum class Xop {
  Check, Literal, Load, Path, Constructor, Closure, Operator, Binary, Negate,
  Call, Pipe, Compose, Jump, Branch, ShortCircuit, EnterScope, Bind, BindRec,
  LeaveScope, Match, Comprehension, Range, List, Tuple, Record, CopyRecord,
  UpdateField, Project, Return
};
struct Xinst {
  Xop op;
  ExprP node;
  std::size_t arg = 0;
  bool tail = false;
};
struct XvmCode {
  std::uint64_t revision = 0;
  std::vector<Xinst> instructions;
  std::vector<std::shared_ptr<native::Executable>> blocks;
  std::size_t native_bytes = 0;
  bool native_attempted = false;
};
struct XvmCompiler {
  Interp &in;
  explicit XvmCompiler(Interp &interp) : in(interp) {}
  std::shared_ptr<XvmCode> code = std::make_shared<XvmCode>();
  std::size_t emit(Xop op, const ExprP &e, std::size_t arg = 0, bool tail = false)
  {
    if (code->instructions.size() >= in.xvm_code_limit)
      in.fail("M0406", "XVM instruction limit exceeded", e->span);
    code->instructions.push_back({op, e, arg, tail});
    return code->instructions.size() - 1;
  }
  void lower(const ExprP &root, bool root_tail)
  {
    enum class Kind { Lower, Emit, Mark };
    struct Task { Kind kind; ExprP node; Xop op; std::size_t arg, label; bool tail; };
    std::vector<Task> work{{Kind::Lower, root, Xop::Check, 0, SIZE_MAX, root_tail}};
    std::vector<std::size_t> labels;
    std::vector<std::pair<std::size_t, std::size_t>> fixes;
    while (!work.empty()) {
      auto task = std::move(work.back()); work.pop_back();
      if (task.kind == Kind::Mark) { labels[task.arg] = code->instructions.size(); continue; }
      if (task.kind == Kind::Emit) {
        auto at = emit(task.op, task.node, task.arg, task.tail);
        if (task.label != SIZE_MAX) fixes.push_back({at, task.label});
        continue;
      }
      auto e = task.node; bool tail = task.tail;
      emit(Xop::Check, e);
      std::vector<Task> sequence;
      auto child = [&](const ExprP &n, bool t = false) { sequence.push_back({Kind::Lower, n, Xop::Check, 0, SIZE_MAX, t}); };
      auto operation = [&](Xop op, const ExprP &n, std::size_t arg = 0, bool t = false, std::size_t label = SIZE_MAX) {
        sequence.push_back({Kind::Emit, n, op, arg, label, t});
      };
      auto label = [&]() { labels.push_back(SIZE_MAX); return labels.size() - 1; };
      auto mark = [&](std::size_t id) { sequence.push_back({Kind::Mark, e, Xop::Check, id, SIZE_MAX, false}); };
      switch (e->kind) {
    case EK::Int: case EK::Float: case EK::Str: case EK::Bool: case EK::Unit:
      operation(Xop::Literal, e); break;
    case EK::Var: case EK::Tacit: operation(Xop::Load, e); break;
    case EK::Path: operation(Xop::Path, e); break;
    case EK::Ctor: operation(Xop::Constructor, e); break;
    case EK::Lambda: operation(Xop::Closure, e); break;
    case EK::OpRef: operation(Xop::Operator, e); break;
    case EK::App:
      if (e->resolution != Resolution::Apply) {
        child(e->kids[0]); child(e->kids[1]); operation(Xop::Compose, e);
      } else {
        std::vector<ExprP> args;
        auto h = e;
        while (h->kind == EK::App && h->resolution == Resolution::Apply) {
          args.push_back(h->kids[1]); h = h->kids[0];
        }
        child(h);
        for (auto i = args.rbegin(); i != args.rend(); ++i) child(*i);
        operation(Xop::Call, e, args.size(), tail);
      }
      break;
    case EK::Pipe:
      child(e->kids[0]); child(e->kids[1]);
      operation(e->resolution == Resolution::Apply ? Xop::Pipe : Xop::Compose, e, 0, tail); break;
    case EK::BinOp:
      child(e->kids[0]);
      if (e->text == "&&" || e->text == "||") {
        auto end = label(); operation(Xop::ShortCircuit, e, 0, false, end);
        child(e->kids[1], tail);
        mark(end);
      } else { child(e->kids[1]); operation(Xop::Binary, e); }
      break;
    case EK::Neg: child(e->kids[0]); operation(Xop::Negate, e); break;
    case EK::If: {
      auto otherwise = label(), end = label();
      child(e->kids[0]); operation(Xop::Branch, e, 0, false, otherwise);
      child(e->kids[1], tail); operation(Xop::Jump, e, 0, false, end);
      mark(otherwise); child(e->kids[2], tail); mark(end); break;
    }
    case EK::Match:
      child(e->kids[0]); operation(Xop::Match, e, 0, tail); break;
    case EK::Let:
      child(e->kids[0]); operation(Xop::Bind, e); child(e->kids[1], tail);
      if (!tail) operation(Xop::LeaveScope, e);
      break;
    case EK::LetRec:
      operation(Xop::EnterScope, e);
      for (std::size_t i = 0; i < e->pats.size(); ++i) { child(e->kids[i]); operation(Xop::BindRec, e, i); }
      child(e->kids.back(), tail); if (!tail) operation(Xop::LeaveScope, e); break;
    case EK::List: case EK::Tuple: case EK::Record:
      for (const auto &k : e->kids) child(k);
      operation(e->kind == EK::List ? Xop::List : e->kind == EK::Tuple ? Xop::Tuple : Xop::Record, e, e->kids.size()); break;
    case EK::Comp: operation(Xop::Comprehension, e); break;
    case EK::Range: child(e->kids[0]); child(e->kids[1]); operation(Xop::Range, e); break;
    case EK::Update:
      child(e->kids[0]); operation(Xop::CopyRecord, e);
      for (std::size_t i = 0; i < e->labels.size(); ++i) { child(e->kids[i + 1]); operation(Xop::UpdateField, e, i); }
      break;
    case EK::Project: child(e->kids[0]); operation(Xop::Project, e); break;
    case EK::Annot: child(e->kids[0], tail); break;
      }
      if (sequence.size() > in.xvm_code_limit || work.size() > in.xvm_code_limit - sequence.size())
        in.fail("M0406", "XVM compiler work limit exceeded", e->span);
      work.insert(work.end(), sequence.rbegin(), sequence.rend());
    }
    for (auto [at, id] : fixes) code->instructions[at].arg = labels.at(id);
  }
};
inline std::shared_ptr<XvmCode> xvm_code(Interp &in, const ExprP &e)
{
  auto found = in.xvm_cache.find(e.get());
  if (found != in.xvm_cache.end() && found->second->revision == in.xvm_revision) {
    if (found->second->instructions.size() > in.xvm_code_limit)
      in.fail("M0406", "XVM instruction limit exceeded", e->span);
    return found->second;
  }
  XvmCompiler compiler{in}; compiler.lower(e, true); compiler.emit(Xop::Return, e);
  auto code = compiler.code;
  code->revision = in.xvm_revision;
  in.xvm_cache[e.get()] = code;
  return code;
}
struct XvmState {
  Interp &in;
  FrameP env;
  bool counted;
  std::shared_ptr<XvmCode> code;
  std::size_t pc = 0;
  std::vector<Value> values;
  std::vector<FrameP> scopes;
  bool done = false;
  Value result = Value::unit();
  std::exception_ptr exception;
  XvmState(Interp &interp, FrameP frame, bool call_counted, std::shared_ptr<XvmCode> compiled)
    : in(interp), env(std::move(frame)), counted(call_counted), code(std::move(compiled)) {}
  Value pop() { auto v = std::move(values.back()); values.pop_back(); return v; }
  void enter(const ExprP &body, FrameP frame)
  {
    code = xvm_code(in, body); env = std::move(frame); pc = 0; values.clear(); scopes.clear();
  }
  Value lookup_operator(const ExprP &e, bool global)
  {
    if (e->cell && e->cell->defined) return e->cell->value;
    if (auto v = env->find_var(e->text)) return *v;
    if (global && in.global_lookup) if (auto c = in.global_lookup(e->text)) return c->value;
    in.fail("M0204", "unbound operator `" + e->text + "` at run time", e->span);
  }
  int call(const Xinst &i, Value function, std::vector<Value> args)
  {
    auto body = i.node;
    if (i.tail && in.tail_call(function, args, body, env, counted)) { enter(body, env); return 1; }
    values.push_back(in.apply(function, std::move(args), i.node->span)); return 0;
  }
  template<Xop op> int execute(const Xinst &i)
  {
    const auto &e = i.node;
    ++pc; ++in.xvm_instructions;
    switch (op) {
    case Xop::Check:
      in.check_cancelled(e->span); break;
    case Xop::Literal:
      switch (e->kind) {
      case EK::Int: values.push_back(Value::integer(e->ival)); break;
      case EK::Float: values.push_back(Value::floating(e->fval)); break;
      case EK::Str: values.push_back(make_str(e->text)); break;
      case EK::Bool: values.push_back(Value::boolean(e->ival != 0)); break;
      default: values.push_back(Value::unit()); break;
      } break;
    case Xop::Load:
      if (e->cell) {
        if (!e->cell->defined) in.fail("M0408", "`" + e->cell->name + "` is used before its definition", e->span);
        values.push_back(e->cell->value);
      } else if (auto v = env->find_var(e->text)) values.push_back(*v);
      else {
        auto c = in.global_lookup ? in.global_lookup(e->text) : nullptr;
        if (!c || !c->defined) in.fail("M0204", "unbound variable `" + e->text + "` at run time", e->span);
        values.push_back(c->value);
      } break;
    case Xop::Path:
      if (e->prim) values.push_back(in.prim_value(e->prim));
      else {
        std::vector<std::string> mp(e->path.begin(), e->path.end() - 1);
        values.push_back(in.module_field(in.resolve_module(mp, env, e->span), e->path.back(), e->span));
      } break;
    case Xop::Constructor:
      if (!e->ctor_arity) values.push_back(make_ctor(e->text, e->ctor_index));
      else {
        auto c = std::make_shared<CtorFnObj>(); c->name = e->text; c->index = e->ctor_index; c->arity = e->ctor_arity;
        Value v; v.kind = VK::CtorFn; v.obj = c; values.push_back(v);
      } break;
    case Xop::Closure: {
      auto c = std::make_shared<ClosureObj>(); c->lam = e; c->env = env;
      Value v; v.kind = VK::Closure; v.obj = c; values.push_back(v); break;
    }
    case Xop::Operator:
      values.push_back(detail::is_builtin_op(e->text) ? in.prim_value(in.op_prim(e->text)) : lookup_operator(e, false)); break;
    case Xop::Binary: {
      auto b = pop(), a = pop();
      values.push_back(detail::is_builtin_op(e->text) ? in.binop(e->text, a, b, e->span)
        : in.apply(lookup_operator(e, true), {a, b}, e->span)); break;
    }
    case Xop::Negate: {
      auto a = pop(); values.push_back(a.kind == VK::Int ? Value::integer(detail::wrap_neg(a.i)) : Value::floating(-a.f)); break;
    }
    case Xop::Call: {
      auto base = values.size() - i.arg - 1;
      auto f = values[base]; std::vector<Value> args(values.begin() + base + 1, values.end());
      values.resize(base); return call(i, f, std::move(args));
    }
    case Xop::Pipe: { auto b = pop(), a = pop(); return call(i, b, {a}); }
    case Xop::Compose: {
      auto b = pop(), a = pop();
      values.push_back(e->resolution == Resolution::ComposeBwd ? in.compose(b, a) : in.compose(a, b)); break;
    }
    case Xop::Jump: pc = i.arg; return 1;
    case Xop::Branch: if (!pop().i) pc = i.arg; return 1;
    case Xop::ShortCircuit:
      if ((e->text == "&&") != (values.back().i != 0)) pc = i.arg;
      else values.pop_back();
      return 1;
    case Xop::EnterScope: scopes.push_back(env); env = Frame::child(env); break;
    case Xop::Bind: {
      auto v = pop(); auto f = Frame::child(env);
      if (!in.match(e->pats[0], v, *f)) in.fail("M0401", "the value `" + show_value(v) + "` does not match the `let` pattern", e->pats[0]->span);
      scopes.push_back(env); env = f; break;
    }
    case Xop::BindRec: env->vars.push_back({e->pats[i.arg]->text, pop()}); break;
    case Xop::LeaveScope: env = scopes.back(); scopes.pop_back(); break;
    case Xop::Match: {
      auto v = pop();
      for (std::size_t k = 0; k < e->pats.size(); ++k) {
        auto f = Frame::child(env);
        if (!in.match(e->pats[k], v, *f) || (e->guards[k] && !in.eval(e->guards[k], f).i)) continue;
        if (i.tail) { enter(e->kids[k + 1], f); return 1; }
        values.push_back(in.eval(e->kids[k + 1], f)); return 0;
      }
      in.fail("M0401", "no pattern matched the value `" + show_value(v) + "`", e->span);
    }
    case Xop::Comprehension: {
      std::vector<Value> out; in.comprehension(e, 0, env, out); values.push_back(make_list(out)); break;
    }
    case Xop::Range: {
      auto b = pop(), a = pop(); std::vector<Value> out;
      if (b.i >= a.i && std::uint64_t(b.i) - std::uint64_t(a.i) > 50000000ULL) in.fail("M0404", "range is too large", e->span);
      for (auto n = a.i; n <= b.i; ++n) { out.push_back(Value::integer(n)); if (n == INT64_MAX) break; }
      values.push_back(make_list(out)); break;
    }
    case Xop::List: case Xop::Tuple: case Xop::Record: {
      auto base = values.size() - i.arg;
      std::vector<Value> items(values.begin() + base, values.end()); values.resize(base);
      if (op == Xop::List) values.push_back(make_list(items));
      else if (op == Xop::Tuple) values.push_back(make_tuple(std::move(items)));
      else {
        std::vector<std::pair<std::string, Value>> fields;
        for (std::size_t k = 0; k < items.size(); ++k) fields.push_back({e->labels[k], items[k]});
        values.push_back(make_record(std::move(fields)));
      } break;
    }
    case Xop::CopyRecord: {
      auto base = pop();
      if (base.kind != VK::Record) in.fail("M0404", "record update on a non-record value", e->span);
      base.obj = std::make_shared<AggObj>(*agg_of(base)); values.push_back(base); break;
    }
    case Xop::UpdateField: {
      auto value = pop(); auto a = static_cast<AggObj *>(values.back().obj.get());
      for (std::size_t k = 0; k < a->labels->size(); ++k) if ((*a->labels)[k] == e->labels[i.arg]) a->items[k] = value;
      break;
    }
    case Xop::Project: {
      auto record = pop(); auto field = record_field(record, e->text);
      if (!field) in.fail("M0404", "the value `" + show_value(record) + "` has no field `" + e->text + "`", e->span);
      values.push_back(*field); break;
    }
    case Xop::Return: result = pop(); done = true; return 1;
    }
    if (values.size() > in.xvm_stack_limit || scopes.size() > in.xvm_stack_limit)
      in.fail("M0406", "XVM operand/environment stack limit exceeded", e->span);
    return 0;
  }
};
template<Xop op> inline int xvm_step(void *state, const void *instruction) noexcept
{
  auto &s = *static_cast<XvmState *>(state);
  try { return s.execute<op>(*static_cast<const Xinst *>(instruction)); }
  catch (...) { s.exception = std::current_exception(); return 1; }
}
using XvmStep = int (*)(void *, const void *);
inline XvmStep xvm_handler(Xop op)
{
#define METAML_XOP(name) case Xop::name: return &xvm_step<Xop::name>
  switch (op) {
    METAML_XOP(Check); METAML_XOP(Literal); METAML_XOP(Load); METAML_XOP(Path);
    METAML_XOP(Constructor); METAML_XOP(Closure); METAML_XOP(Operator); METAML_XOP(Binary);
    METAML_XOP(Negate); METAML_XOP(Call); METAML_XOP(Pipe); METAML_XOP(Compose);
    METAML_XOP(Jump); METAML_XOP(Branch); METAML_XOP(ShortCircuit); METAML_XOP(EnterScope);
    METAML_XOP(Bind); METAML_XOP(BindRec); METAML_XOP(LeaveScope); METAML_XOP(Match);
    METAML_XOP(Comprehension); METAML_XOP(Range); METAML_XOP(List); METAML_XOP(Tuple);
    METAML_XOP(Record); METAML_XOP(CopyRecord); METAML_XOP(UpdateField); METAML_XOP(Project); METAML_XOP(Return);
  }
#undef METAML_XOP
  throw std::runtime_error("unknown XVM opcode");
}
inline bool xvm_terminator(const Xinst &i)
{
  return i.op == Xop::Jump || i.op == Xop::Branch || i.op == Xop::ShortCircuit || i.op == Xop::Return
    || (i.tail && (i.op == Xop::Call || i.op == Xop::Pipe || i.op == Xop::Match));
}
inline void xvm_native(Interp &in, const std::shared_ptr<XvmCode> &code)
{
  if (code->native_attempted) return;
  if (native::host() == native::Target::Unsupported || !in.xvm_native_limit) return;
  auto n = code->instructions.size();
  std::vector<bool> leader(n, false); leader[0] = true;
  for (std::size_t i = 0; i < n; ++i) {
    const auto &instruction = code->instructions[i];
    if (instruction.op == Xop::Jump || instruction.op == Xop::Branch || instruction.op == Xop::ShortCircuit) leader.at(instruction.arg) = true;
    if (xvm_terminator(instruction) && i + 1 < n) leader[i + 1] = true;
  }
  std::vector<std::shared_ptr<native::Executable>> blocks(n);
  std::size_t bytes = 0;
  try {
    for (std::size_t begin = 0; begin < n;) {
      std::vector<native::Call> calls;
      std::size_t end = begin;
      do {
        const auto &i = code->instructions[end];
        calls.push_back({reinterpret_cast<std::uintptr_t>(xvm_handler(i.op)), reinterpret_cast<std::uintptr_t>(&i)});
        ++end;
        if (xvm_terminator(i)) break;
      } while (end < n && !leader[end] && calls.size() < 32);
      auto machine_code = native::block(native::host(), calls);
      if (machine_code.size() > in.xvm_native_limit - bytes)
        in.fail("M0406", "XVM native code limit exceeded", code->instructions[begin].node->span);
      bytes += machine_code.size();
      blocks[begin] = std::make_shared<native::Executable>(machine_code);
      begin = end;
    }
  } catch (const std::runtime_error &) { code->native_attempted = true; return; } // denied executable mappings
  code->native_attempted = true;
  code->blocks = std::move(blocks); code->native_bytes = bytes;
}
inline Value eval_xvm(Interp &in, ExprP e, FrameP env, bool counted)
{
  XvmState state{in, std::move(env), counted, {}};
  struct Guard {
    XvmState &s;
    Guard(XvmState &s) : s(s) { ++s.in.eval_depth; }
    ~Guard() { --s.in.eval_depth; if (s.counted && s.in.call_depth > 0) --s.in.call_depth; }
  } guard(state);
  if (in.eval_depth > in.max_depth * 8 + 1000) in.fail("M0406", "stack overflow: expression nesting too deep", e->span);
  state.code = xvm_code(in, e);
  while (!state.done) {
    auto code = state.code; // retain native instructions across tail transfers
    if (in.engine != ExecutionEngine::PortableXVM) xvm_native(in, code);
    if (in.engine == ExecutionEngine::NativeXVM && (code->blocks.empty() || !in.xvm_native_limit))
      in.fail("M0409", "XVM native executable memory is unavailable", e->span);
    if (!code->blocks.empty() && in.xvm_native_limit && in.engine != ExecutionEngine::PortableXVM) {
      if (code->native_bytes > in.xvm_native_limit) in.fail("M0406", "XVM native code limit exceeded", e->span);
      auto block = code->blocks.at(state.pc);
      if (!block) throw std::runtime_error("invalid XVM block entry");
      ++in.xvm_native_blocks; block->entry<int (*)(void *)>()(&state);
    } else {
      const auto &i = code->instructions.at(state.pc);
      xvm_handler(i.op)(&state, &i);
    }
    if (state.exception) std::rethrow_exception(state.exception);
  }
  return state.result;
}
}
