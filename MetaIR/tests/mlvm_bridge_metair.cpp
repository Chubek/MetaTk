#include "MLVM/include/MLVM-MetaIR.hpp"
#include <cstdio>

extern "C" int mlvm_bridge_metair_run(const unsigned char* bytes, size_t length, size_t steps, int64_t expected) {
    auto decoded = metail::decode(std::span<const std::uint8_t>(bytes, length));
    if (!decoded) return 0;
    metavm::VMOptions limits; limits.max_steps = steps;
    auto result = metail::run(decoded.program, limits);
    return result && result.value == expected;
}

extern "C" int mlvm_bridge_metair_tests() {
    std::unique_ptr<mlvm_context, decltype(&mlvm_destroy)> context(mlvm_create(), mlvm_destroy);
    if (!context) return 0;
    auto parsed = metail::parse("const r0, -27\nconst r1, 3\nadd r2, r0, r1\nsub r2, r0, r1\n"
        "mul r2, r0, r1\ndiv r2, r0, r1\nmod r2, r0, r1\nand r2, r0, r1\nor r2, r0, r1\n"
        "xor r2, r0, r1\nshl r2, r0, r1\nshr r2, r0, r1\neq r2, r0, r1\nlt r2, r0, r1\n"
        "jz r2, 16\njmp 16\nhalt r2\n", "bridge.mcil");
    if (!parsed) return 0;
    for (int engine = MLVM_INTERPRETER; engine <= MLVM_JIT; ++engine) {
        if (mlvm_set_engine(context.get(), engine) != MLVM_OK) return 0;
        auto imported = mlvm::metair::import_program(context.get(), parsed.program);
        if (!imported) { std::fprintf(stderr, "%s\n", imported.diagnostics.front().message.c_str()); return 0; }
        auto exported = mlvm::metair::export_program(context.get(), imported->get());
        if (!exported || exported->code != parsed.program.code || !exported->source_map.empty()) return 0;
        auto executed = metail::run(*exported);
        int64_t result = 0;
        if (!executed || executed.value != 1 || mlvm_run(context.get(), imported->get(), 100, &result) != MLVM_OK || result != 1) return 0;
        mlvm_module* raw = nullptr;
        if (mlvm_optimize(context.get(), imported->get(), MLVM_OPT_GLOBAL, &raw) != MLVM_OK) return 0;
        mlvm::metair::Module optimized(raw);
        auto lowered = mlvm::metair::export_program(context.get(), optimized.get());
        if (!lowered || lowered->size() != parsed.program.size() || !metail::run(*lowered) || metail::run(*lowered).value != 1) return 0;
    }
    // Reject MetaIL-only semantics even when MetaIL's verifier accepts them.
    auto unsupported = metail::parse("nop\nconst r0, 1\nhalt r0\n");
    if (!unsupported || mlvm::metair::import_program(context.get(), unsupported.program)) return 0;
    auto uninitialized = metail::parse("halt r0\n");
    if (!uninitialized || mlvm::metair::import_program(context.get(), uninitialized.program)) return 0;
    auto malformed = parsed.program;
    malformed.code[0].a = 32;
    auto rejected = mlvm::metair::import_program(context.get(), malformed);
    if (rejected || rejected.diagnostics.front().stage != metachain::Stage::Verify) return 0;
    metail::Program oversized;
    oversized.code.resize(MLVM_BRIDGE_MAX_INSTRUCTIONS + 1);
    rejected = mlvm::metair::import_program(context.get(), oversized);
    if (rejected || rejected.diagnostics.front().message != "instruction limit exceeded") return 0;
    if (mlvm::metair::export_program(nullptr, nullptr)) return 0;

    // Bit-exact signed immediates, including the high bit and all-ones.
    for (int64_t n : {INT64_MIN, INT64_MAX, INT64_C(-1), INT64_C(0)}) {
        metail::Program program;
        program.code = {{metail::Op::Const, 31, 0, 0, n}, {metail::Op::Halt, 31, 0, 0, 0}};
        auto imported = mlvm::metair::import_program(context.get(), program);
        if (!imported) return 0;
        auto exported = mlvm::metair::export_program(context.get(), imported->get());
        if (!exported || exported->code != program.code) return 0;
        int64_t result = 0;
        if (mlvm_run(context.get(), imported->get(), 2, &result) != MLVM_OK || result != n) return 0;
    }
    return 1;
}
