# MLVM

The first integer core is implemented **in QScript**. See the
[manual](../manual/mlvm.md) for IR semantics, library layers, CLI, tests and
the CFG/dominance/liveness analyses, local/global constant propagation,
interference allocation and exact AMD64/RV64 backend boundaries. Other source and dialect placeholders
are not implementations.

```sh
python3 MLVM/tooling/mlvm.py run MLVM/tests/sum.mlvm --optimize
python3 MLVM/tooling/mlvm.py analyze MLVM/tests/sum.mlvm
python3 MLVM/tooling/mlvm.py opt MLVM/tests/diamond.mlvm --global-optimize
python3 tests/mlvm.py build/qscript cc
python3 tests/mlvm_dataflow.py build/qscript
```

The [MetaIR ↔ MLVM bridge](../manual/mlvm-bridge.md) provides a C99 embedding
API backed by embedded QScript, MCIL import/export, and an optional C++ adapter
for `metail::Program`. Link the CMake target `MLVM::Bridge`.
