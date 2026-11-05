module XVMCode;
import MetaML;
import XVMNative : Native;
version(METAML_ENABLE_QBE) import QBENative : QBE;
import std.typecons : Tuple, tuple;
template XVMEngine() {
    alias N = Native!();
    version(METAML_ENABLE_QBE) alias Q = QBE!();
    enum Op {
        Check, Literal, Load, Path, Constructor, Closure, Operator, Binary, Negate,
        Call, Pipe, Compose, Jump, Branch, ShortCircuit, EnterScope, Bind, BindRec,
        LeaveScope, Match, Comprehension, Range, List, Tuple, Record, CopyRecord,
        UpdateField, Project, Return
    }
    struct Instruction { Op op; Expr node; size_t arg; bool tail; }
    class Code {
        ulong revision;
        Instruction[] instructions;
        N.Executable[] blocks;
        size_t nativeBytes;
        bool nativeAttempted;
        version(METAML_ENABLE_QBE) {
            Q.Module qbeModule;
            size_t[] qbeBlocks;
            size_t qbeBytes;
        }
        void releaseQBE() {
            version(METAML_ENABLE_QBE) {
                if (qbeModule !is null) destroy(qbeModule);
                qbeModule = null;
            }
        }
    }
    struct Compiler {
        Interp in_;
        Code code;
        size_t emit(Op op, Expr e, size_t arg = 0, bool tail = false) {
            if (code.instructions.length >= in_.xvm_code_limit) in_.fail("M0406", "XVM instruction limit exceeded", e.span);
            code.instructions ~= Instruction(op, e, arg, tail);
            return code.instructions.length - 1;
        }
        void lower(Expr root, bool rootTail) {
            enum Kind { Lower, Emit, Mark }
            struct Task { Kind kind; Expr node; Op op; size_t arg, label; bool tail; }
            Task[] work = [Task(Kind.Lower, root, Op.Check, 0, size_t.max, rootTail)];
            size_t[] labels;
            Tuple!(size_t, size_t)[] fixes;
            while (work.length) {
                auto task = work[$ - 1]; work.length--;
                if (task.kind == Kind.Mark) { labels[task.arg] = code.instructions.length; continue; }
                if (task.kind == Kind.Emit) {
                    auto at = emit(task.op, task.node, task.arg, task.tail);
                    if (task.label != size_t.max) fixes ~= tuple(at, task.label);
                    continue;
                }
                auto e = task.node; bool tail = task.tail;
                emit(Op.Check, e);
                Task[] sequence;
                void child(Expr n, bool t = false) { sequence ~= Task(Kind.Lower, n, Op.Check, 0, size_t.max, t); }
                void operation(Op op, Expr n, size_t arg = 0, bool t = false, size_t label = size_t.max) {
                    sequence ~= Task(Kind.Emit, n, op, arg, label, t);
                }
                size_t newLabel() { labels ~= size_t.max; return labels.length - 1; }
                void mark(size_t id) { sequence ~= Task(Kind.Mark, e, Op.Check, id, size_t.max, false); }
                final switch(e.kind) {
            case EK.Int, EK.Float, EK.Str, EK.Bool, EK.Unit: operation(Op.Literal, e); break;
            case EK.Var, EK.Tacit: operation(Op.Load, e); break;
            case EK.Path: operation(Op.Path, e); break;
            case EK.Ctor: operation(Op.Constructor, e); break;
            case EK.Lambda: operation(Op.Closure, e); break;
            case EK.OpRef: operation(Op.Operator, e); break;
            case EK.App:
                if (e.resolution != Resolution.Apply) {
                    child(e.kids[0]); child(e.kids[1]); operation(Op.Compose, e);
                } else {
                    Expr[] args; auto h = e;
                    while (h.kind == EK.App && h.resolution == Resolution.Apply) { args ~= h.kids[1]; h = h.kids[0]; }
                    child(h);
                    foreach_reverse(arg; args) child(arg);
                    operation(Op.Call, e, args.length, tail);
                } break;
            case EK.Pipe:
                child(e.kids[0]); child(e.kids[1]); operation(e.resolution == Resolution.Apply ? Op.Pipe : Op.Compose, e, 0, tail); break;
            case EK.BinOp:
                child(e.kids[0]);
                if (e.text == "&&" || e.text == "||") {
                    auto end = newLabel(); operation(Op.ShortCircuit, e, 0, false, end); child(e.kids[1], tail); mark(end);
                } else { child(e.kids[1]); operation(Op.Binary, e); }
                break;
            case EK.Neg: child(e.kids[0]); operation(Op.Negate, e); break;
            case EK.If:
                auto otherwise = newLabel(), end = newLabel();
                child(e.kids[0]); operation(Op.Branch, e, 0, false, otherwise);
                child(e.kids[1], tail); operation(Op.Jump, e, 0, false, end);
                mark(otherwise); child(e.kids[2], tail); mark(end); break;
            case EK.Match: child(e.kids[0]); operation(Op.Match, e, 0, tail); break;
            case EK.Let:
                child(e.kids[0]); operation(Op.Bind, e); child(e.kids[1], tail);
                if (!tail) operation(Op.LeaveScope, e); break;
            case EK.LetRec:
                operation(Op.EnterScope, e);
                foreach (i; 0 .. e.pats.length) { child(e.kids[i]); operation(Op.BindRec, e, i); }
                child(e.kids[$ - 1], tail); if (!tail) operation(Op.LeaveScope, e); break;
            case EK.List, EK.Tuple, EK.Record:
                foreach (k; e.kids) child(k);
                operation(e.kind == EK.List ? Op.List : e.kind == EK.Tuple ? Op.Tuple : Op.Record, e, e.kids.length); break;
            case EK.Comp: operation(Op.Comprehension, e); break;
            case EK.Range: child(e.kids[0]); child(e.kids[1]); operation(Op.Range, e); break;
            case EK.Update:
                child(e.kids[0]); operation(Op.CopyRecord, e);
                foreach (i; 0 .. e.labels.length) { child(e.kids[i + 1]); operation(Op.UpdateField, e, i); } break;
            case EK.Project: child(e.kids[0]); operation(Op.Project, e); break;
            case EK.Annot: child(e.kids[0], tail); break;
                }
                if (sequence.length > in_.xvm_code_limit || work.length > in_.xvm_code_limit - sequence.length)
                    in_.fail("M0406", "XVM compiler work limit exceeded", e.span);
                foreach_reverse(item; sequence) work ~= item;
            }
            foreach (fix; fixes) code.instructions[fix[0]].arg = labels[fix[1]];
        }
    }
    Code getCode(Interp in_, Expr e) {
        if (auto found = e in in_.xvm_cache) {
            if ((*found).revision == in_.xvm_revision) {
                if ((*found).instructions.length > in_.xvm_code_limit) in_.fail("M0406", "XVM instruction limit exceeded", e.span);
                return *found;
            }
        }
        auto compiler = Compiler(in_, new Code);
        compiler.lower(e, true); compiler.emit(Op.Return, e);
        compiler.code.revision = in_.xvm_revision;
        in_.xvm_cache[e] = compiler.code;
        return compiler.code;
    }
    struct State {
        Interp in_;
        Frame env;
        bool counted;
        Code code;
        size_t pc;
        Value[] values;
        Frame[] scopes;
        bool done;
        Value result;
        Throwable exception;
        Value pop() { auto v = values[$ - 1]; values.length--; return v; }
        void enter(Expr body, Frame frame) { code = getCode(in_, body); env = frame; pc = 0; values = null; scopes = null; }
        Value lookupOperator(Expr e, bool global) {
            if (e.cell !is null && e.cell.defined) return e.cell.value;
            if (auto v = env.find_var(e.text)) return *v;
            if (global && in_.global_lookup !is null) if (auto c = in_.global_lookup(e.text)) return c.value;
            in_.fail("M0204", "unbound operator `" ~ e.text ~ "` at run time", e.span);
        }
        int call(ref Instruction i, Value f, Value[] args) {
            auto body = i.node;
            if (i.tail && in_.xvm_tail_call(f, args, body, env, counted)) { enter(body, env); return 1; }
            values ~= in_.apply(f, args, i.node.span); return 0;
        }
        int execute(Op op)(ref Instruction i) {
            auto e = i.node; ++pc; ++in_.xvm_instructions;
            switch(op) {
            case Op.Check: in_.check_cancelled(e.span); break;
            case Op.Literal:
                switch(e.kind) {
                case EK.Int: values ~= Value.integer(e.ival); break;
                case EK.Float: values ~= Value.floating(e.fval); break;
                case EK.Str: values ~= make_str(e.text); break;
                case EK.Bool: values ~= Value.boolean(e.ival != 0); break;
                default: values ~= Value.unit(); break;
                } break;
            case Op.Load:
                if (e.cell !is null) {
                    if (!e.cell.defined) in_.fail("M0408", "`" ~ e.cell.name ~ "` is used before its definition", e.span);
                    values ~= e.cell.value;
                } else if (auto v = env.find_var(e.text)) values ~= *v;
                else {
                    auto c = in_.global_lookup is null ? null : in_.global_lookup(e.text);
                    if (c is null || !c.defined) in_.fail("M0204", "unbound variable `" ~ e.text ~ "` at run time", e.span);
                    values ~= c.value;
                } break;
            case Op.Path:
                if (e.prim !is null) values ~= in_.prim_value(e.prim);
                else values ~= in_.module_field(in_.resolve_module(e.path[0 .. $ - 1], env, e.span), e.path[$ - 1], e.span);
                break;
            case Op.Constructor:
                if (!e.ctor_arity) values ~= make_ctor(e.text, e.ctor_index);
                else {
                    auto c = new CtorFnObj; c.name = e.text; c.index = e.ctor_index; c.arity = e.ctor_arity;
                    Value v; v.kind = VK.CtorFn; v.obj = c; values ~= v;
                } break;
            case Op.Closure:
                auto c = new ClosureObj; c.lam = e; c.env = env;
                Value v; v.kind = VK.Closure; v.obj = c; values ~= v; break;
            case Op.Operator: values ~= is_builtin_op(e.text) ? in_.prim_value(in_.op_prim(e.text)) : lookupOperator(e, false); break;
            case Op.Binary:
                auto b = pop(), a = pop();
                values ~= is_builtin_op(e.text) ? in_.binop(e.text, a, b, e.span) : in_.apply(lookupOperator(e, true), [a, b], e.span); break;
            case Op.Negate:
                auto a = pop(); values ~= a.kind == VK.Int ? Value.integer(wrap_neg(a.i)) : Value.floating(-a.f); break;
            case Op.Call:
                auto base = values.length - i.arg - 1;
                auto f = values[base]; auto args = values[base + 1 .. $].dup; values = values[0 .. base]; return call(i, f, args);
            case Op.Pipe:
                auto b = pop(), a = pop(); return call(i, b, [a]);
            case Op.Compose:
                auto b = pop(), a = pop(); values ~= e.resolution == Resolution.ComposeBwd ? in_.compose(b, a) : in_.compose(a, b); break;
            case Op.Jump: pc = i.arg; return 1;
            case Op.Branch: if (!pop().i) pc = i.arg; return 1;
            case Op.ShortCircuit:
                if ((e.text == "&&") != (values[$ - 1].i != 0)) pc = i.arg;
                else values.length--;
                return 1;
            case Op.EnterScope: scopes ~= env; env = Frame.child(env); break;
            case Op.Bind:
                auto value = pop(); auto frame = Frame.child(env);
                if (!in_.match(e.pats[0], value, frame)) in_.fail("M0401", "the value `" ~ show_value(value) ~ "` does not match the `let` pattern", e.pats[0].span);
                scopes ~= env; env = frame; break;
            case Op.BindRec: env.vars ~= tuple(e.pats[i.arg].text, pop()); break;
            case Op.LeaveScope: env = scopes[$ - 1]; scopes.length--; break;
            case Op.Match:
                auto value = pop();
                foreach (k; 0 .. e.pats.length) {
                    auto frame = Frame.child(env);
                    if (!in_.match(e.pats[k], value, frame) || (e.guards[k] !is null && !in_.eval(e.guards[k], frame).i)) continue;
                    if (i.tail) { enter(e.kids[k + 1], frame); return 1; }
                    values ~= in_.eval(e.kids[k + 1], frame); return 0;
                }
                in_.fail("M0401", "no pattern matched the value `" ~ show_value(value) ~ "`", e.span);
            case Op.Comprehension:
                Value[] output; in_.xvm_comprehension(e, 0, env, output); values ~= make_list(output); break;
            case Op.Range:
                auto b = pop(), a = pop(); Value[] output;
                if (b.i >= a.i && cast(ulong)b.i - cast(ulong)a.i > 50_000_000UL) in_.fail("M0404", "range is too large", e.span);
                for (auto n = a.i; n <= b.i; ++n) { output ~= Value.integer(n); if (n == long.max) break; }
                values ~= make_list(output); break;
            case Op.List, Op.Tuple, Op.Record:
                auto base = values.length - i.arg; auto items = values[base .. $].dup; values = values[0 .. base];
                if (op == Op.List) values ~= make_list(items);
                else if (op == Op.Tuple) values ~= make_tuple(items);
                else {
                    Tuple!(string, Value)[] fields;
                    foreach (k; 0 .. items.length) fields ~= tuple(e.labels[k], items[k]);
                    values ~= make_record(fields);
                } break;
            case Op.CopyRecord:
                auto base = pop();
                if (base.kind != VK.Record) in_.fail("M0404", "record update on a non-record value", e.span);
                auto old = agg_of(base), copy = new AggObj;
                copy.tag = old.tag; copy.items = old.items.dup; copy.labels = old.labels; base.obj = copy; values ~= base; break;
            case Op.UpdateField:
                auto value = pop(); auto aggregate = agg_of(values[$ - 1]);
                foreach (k; 0 .. aggregate.labels.length) if (aggregate.labels[k] == e.labels[i.arg]) aggregate.items[k] = value;
                break;
            case Op.Project:
                auto record = pop(); auto field = record_field(record, e.text);
                if (field is null) in_.fail("M0404", "the value `" ~ show_value(record) ~ "` has no field `" ~ e.text ~ "`", e.span);
                values ~= *field; break;
            case Op.Return: result = pop(); done = true; return 1;
            default: throw new Exception("unknown XVM opcode");
            }
            if (values.length > in_.xvm_stack_limit || scopes.length > in_.xvm_stack_limit)
                in_.fail("M0406", "XVM operand/environment stack limit exceeded", e.span);
            return 0;
        }
    }
    extern(C) int step(Op op)(void* state, const void* instruction) nothrow {
        auto s = cast(State*)state;
        try return s.execute!op(*cast(Instruction*)instruction);
        catch (Throwable e) { s.exception = e; return 1; }
    }
    alias Step = extern(C) int function(void*, const void*) nothrow;
    Step handler(Op op) {
        import std.meta : AliasSeq;
        static foreach (value; __traits(allMembers, Op)) {
            if (op == __traits(getMember, Op, value)) return &step!(__traits(getMember, Op, value));
        }
        throw new Exception("unknown XVM opcode");
    }
    bool terminator(ref const Instruction i) {
        return i.op == Op.Jump || i.op == Op.Branch || i.op == Op.ShortCircuit || i.op == Op.Return
            || (i.tail && (i.op == Op.Call || i.op == Op.Pipe || i.op == Op.Match));
    }
    void nativeCode(Interp in_, Code code) {
        if (code.nativeAttempted) return;
        if (N.host() == N.Target.Unsupported || !in_.xvm_native_limit) return;
        auto n = code.instructions.length;
        auto leader = new bool[n]; leader[0] = true;
        foreach (i, ref instruction; code.instructions) {
            if (instruction.op == Op.Jump || instruction.op == Op.Branch || instruction.op == Op.ShortCircuit) leader[instruction.arg] = true;
            if (terminator(instruction) && i + 1 < n) leader[i + 1] = true;
        }
        auto blocks = new N.Executable[n]; size_t bytes;
        try {
            for (size_t begin; begin < n;) {
                N.Call[] calls; auto end = begin;
                do {
                    auto instruction = &code.instructions[end];
                    calls ~= N.Call(cast(size_t)handler(instruction.op), cast(size_t)instruction); ++end;
                    if (terminator(*instruction)) break;
                } while (end < n && !leader[end] && calls.length < 32);
                auto machineCode = N.block(N.host(), calls);
                if (machineCode.length > in_.xvm_native_limit - bytes) in_.fail("M0406", "XVM native code limit exceeded", code.instructions[begin].node.span);
                bytes += machineCode.length; blocks[begin] = new N.Executable(machineCode); begin = end;
            }
        } catch (MetaError e) { foreach (b; blocks) if (b !is null) destroy(b); throw e; }
        catch (Exception e) { foreach (b; blocks) if (b !is null) destroy(b); code.nativeAttempted = true; return; }
        code.nativeAttempted = true;
        code.blocks = blocks; code.nativeBytes = bytes;
    }
    void qbeCode(Interp in_, Code code) {
        auto span = code.instructions[0].node.span;
        version(METAML_NO_JIT) {
            in_.fail("M0409", "QBE native execution is unavailable", span);
        } else version(METAML_ENABLE_QBE) {
            if (!in_.xvm_native_limit || N.host() == N.Target.Unsupported)
                in_.fail("M0409", "QBE native execution is unavailable", span);
            if (code.qbeModule is null) {
                auto n = code.instructions.length;
                auto leader = new bool[n]; leader[0] = true;
                foreach (i, ref instruction; code.instructions) {
                    if (instruction.op == Op.Jump || instruction.op == Op.Branch || instruction.op == Op.ShortCircuit)
                        leader[instruction.arg] = true;
                    if (terminator(instruction) && i + 1 < n) leader[i + 1] = true;
                }
                Q.Call[] calls;
                size_t[] offsets = [0];
                auto blocks = new size_t[n]; blocks[] = size_t.max;
                for (size_t begin; begin < n;) {
                    blocks[begin] = offsets.length - 1;
                    auto end = begin;
                    do {
                        auto instruction = &code.instructions[end];
                        calls ~= Q.Call(cast(size_t)handler(instruction.op), cast(size_t)instruction);
                        ++end;
                        if (terminator(*instruction)) break;
                    } while (end < n && !leader[end] && end - begin < 32);
                    offsets ~= calls.length; begin = end;
                }
                void* module_;
                char[4096] error;
                auto status = Q.metaml_qbe_compile(calls.ptr, offsets.ptr, offsets.length - 1,
                    in_.xvm_native_limit, &module_, error.ptr, error.length);
                if (status == 2) in_.fail("M0406", "QBE native code limit exceeded", span);
                if (status) {
                    import std.string : fromStringz;
                    auto message = fromStringz(error.ptr).idup;
                    in_.fail("M0409", message.length ? message : "QBE compilation failed", span);
                }
                scope(failure) Q.metaml_qbe_release(module_);
                code.qbeModule = new Q.Module(module_);
                code.qbeBytes = Q.metaml_qbe_size(module_);
                code.qbeBlocks = blocks;
            }
            if (code.qbeBytes > in_.xvm_native_limit)
                in_.fail("M0406", "QBE native code limit exceeded", span);
        } else {
            in_.fail("M0409", "QBE backend is not enabled in this build", span);
        }
    }
    Value evaluate(Interp in_, Expr e, Frame env, bool counted) {
        auto state = State(in_, env, counted, null);
        ++in_.eval_depth;
        scope(exit) { --in_.eval_depth; if (state.counted && in_.call_depth > 0) --in_.call_depth; }
        if (in_.eval_depth > in_.max_depth * 8 + 1000) in_.fail("M0406", "stack overflow: expression nesting too deep", e.span);
        state.code = getCode(in_, e);
        while (!state.done) {
            auto code = state.code;
            if (in_.engine == ExecutionEngine.QBE) {
                qbeCode(in_, code);
                version(METAML_ENABLE_QBE) {
                    if (code.qbeBlocks[state.pc] == size_t.max) throw new Exception("invalid QBE block entry");
                    ++in_.xvm_native_blocks; ++in_.qbe_native_blocks;
                    code.qbeModule.call(code.qbeBlocks[state.pc], &state);
                }
            } else {
                if (in_.engine != ExecutionEngine.PortableXVM) nativeCode(in_, code);
                if (in_.engine == ExecutionEngine.NativeXVM && (!code.blocks.length || !in_.xvm_native_limit)) in_.fail("M0409", "XVM native executable memory is unavailable", e.span);
                if (code.blocks.length && in_.xvm_native_limit && in_.engine != ExecutionEngine.PortableXVM) {
                    if (code.nativeBytes > in_.xvm_native_limit) in_.fail("M0406", "XVM native code limit exceeded", e.span);
                    auto block = code.blocks[state.pc];
                    if (block is null) throw new Exception("invalid XVM block entry");
                    ++in_.xvm_native_blocks; block.call(&state);
                } else {
                    auto instruction = &code.instructions[state.pc]; handler(instruction.op)(&state, instruction);
                }
            }
            if (state.exception !is null) throw state.exception;
        }
        return state.result;
    }
}
