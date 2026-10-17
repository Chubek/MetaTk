// Integer arrays for the engine. QScript has no arrays, so each one is a
// handle to storage owned by the host (the carbine::arr_* natives). Every
// vector is freed by the code that created it.

fn vec_new(n) { return carbine::arr_new(n); }
fn vec_get(v, i) { return carbine::arr_get(v, i); }
fn vec_set(v, i, x) { return carbine::arr_set(v, i, x); }
fn vec_len(v) { return carbine::arr_len(v); }
fn vec_free(v) { return carbine::arr_free(v); }

fn vec_fill(v, x) {
    for (let i = 0; i < vec_len(v); i++) vec_set(v, i, x);
    return 0;
}
