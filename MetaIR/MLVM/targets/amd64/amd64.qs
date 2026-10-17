use backend;

// Delegate to QScript's Arch-driven ASMtk/LDtk backend. This returns QScript
// source; `qscript build` or `object` then owns ABI lowering and encoding.
fn lower(code: list<list<int>>, max_steps): str {
    return backend::qscript(code, max_steps);
}
