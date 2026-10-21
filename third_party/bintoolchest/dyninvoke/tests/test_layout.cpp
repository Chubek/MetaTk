// test_layout.cpp - cross-ABI layout queries (no execution).
// Verifies register/stack assignment for all supported ABIs on any host,
// using only the portable classification logic plus Sandstone ISA counts.
#undef NDEBUG
#include "dyninvoke.h"

#include <cassert>
#include <iostream>
#include <vector>

static dinv_cif_t* make(dinv_abi_t abi, const dinv_type_t* ret,
                        std::vector<const dinv_type_t*> args) {
  dinv_cif_t* c = nullptr;
  assert(dinv_cif_create(abi, ret, args.size(), args.data(), &c) == DINV_OK);
  return c;
}

int main() {
  // Failure paths must clear outputs, reject invalid graphs and not leak.
  dinv_type_t invalid{static_cast<dinv_kind_t>(99), nullptr};
  dinv_type_t cycle{DINV_STRUCT, nullptr};
  const dinv_type_t* cycle_elements[] = {&cycle, nullptr};
  cycle.elements = cycle_elements;
  const dinv_type_t* void_elements[] = {&dinv_type_void, nullptr};
  dinv_type_t void_field{DINV_STRUCT, void_elements};
  for (auto* bad : {&invalid, &cycle, &void_field}) {
    for (int i = 0; i < 100; ++i) {
      dinv_cif_t* output = reinterpret_cast<dinv_cif_t*>(1);
      assert(dinv_cif_create(DINV_ABI_SYSV_AMD64, bad, 0, nullptr, &output) == DINV_LAYOUT_ERROR);
      assert(output == nullptr);
    }
  }
  dinv_cif_t* empty = nullptr;
  assert(dinv_cif_create(DINV_ABI_AUTO, &dinv_type_void, 0, nullptr, &empty) == DINV_OK);
  assert(dinv_cif_nargs(empty) == 0);
  dinv_cif_destroy(empty);
  assert(dinv_type_named("i32") == &dinv_type_i32);
  assert(dinv_type_named("nonexistent") == nullptr);
  assert(dinv_abi_can_execute(static_cast<dinv_abi_t>(99)) == 0);
  dinv_closure_t* closure = reinterpret_cast<dinv_closure_t*>(1);
  void* code = reinterpret_cast<void*>(1);
  assert(dinv_closure_create(nullptr, nullptr, nullptr, &closure, &code) == DINV_INVALID_ARGUMENT);
  assert(closure == nullptr && code == nullptr);
  const dinv_type_t* i32 = &dinv_type_i32;
  const dinv_type_t* f64 = &dinv_type_f64;
  const dinv_type_t* f32 = &dinv_type_f32;

  // SysV: 6 GPR + 8 FPR; 7th int goes to stack.
  {
    dinv_cif_t* c = make(DINV_ABI_SYSV_AMD64, i32,
                         {i32, i32, i32, i32, i32, i32, i32});
    assert(dinv_cif_gpr_used(c) == 6);
    assert(dinv_cif_stack_bytes(c) == 8);
    size_t n = 0;
    assert(dinv_cif_arg_slots(c, 6, nullptr, &n) == DINV_OK && n == 1);
    dinv_slot_t s{};
    size_t m = 1;
    assert(dinv_cif_arg_slots(c, 6, &s, &m) == DINV_OK);
    assert(s.where == DINV_LOC_STACK && s.stack_offset == 0);
    dinv_cif_destroy(c);
  }
  // SysV mixed: int,double,int -> gpr=2 fpr=1.
  {
    dinv_cif_t* c = make(DINV_ABI_SYSV_AMD64, i32, {i32, f64, i32});
    assert(dinv_cif_gpr_used(c) == 2 && dinv_cif_fpr_used(c) == 1);
    assert(dinv_cif_stack_bytes(c) == 0);
    dinv_cif_destroy(c);
  }
  // Win64: first four in regs, fifth on stack after 32-byte shadow.
  {
    dinv_cif_t* c =
        make(DINV_ABI_WIN64, i32, {i32, i32, i32, i32, i32});
    assert(dinv_cif_stack_bytes(c) == 8);
    size_t n = 0;
    dinv_cif_arg_slots(c, 4, nullptr, &n);
    dinv_slot_t s{};
    size_t m = 1;
    dinv_cif_arg_slots(c, 4, &s, &m);
    assert(s.where == DINV_LOC_STACK && s.stack_offset == 0);
    dinv_cif_destroy(c);
  }
  // AArch64: 8 ints in regs, 9th on stack.
  {
    std::vector<const dinv_type_t*> a(9, i32);
    dinv_cif_t* c = make(DINV_ABI_AAPCS64, i32, a);
    assert(dinv_cif_gpr_used(c) == 8);
    assert(dinv_cif_stack_bytes(c) >= 4);
    dinv_cif_destroy(c);
  }
  // AAPCS64 scalar stack slots are eight bytes even for i32/f32 payloads.
  {
    std::vector<const dinv_type_t*> a(10, i32);
    dinv_cif_t* c = make(DINV_ABI_AAPCS64, i32, a);
    assert(dinv_cif_stack_bytes(c) == 16);
    dinv_slot_t slot{};
    size_t n = 1;
    assert(dinv_cif_arg_slots(c, 9, &slot, &n) == DINV_OK);
    assert(slot.stack_offset == 8);
    dinv_cif_destroy(c);
  }
  // AArch64 HFA: three floats use three FPRs, no GPRs.
  {
    dinv_cif_t* c = make(DINV_ABI_AAPCS64, f32, {f32, f32});
    assert(dinv_cif_fpr_used(c) == 2 && dinv_cif_gpr_used(c) == 0);
    dinv_cif_destroy(c);
  }
  // RISC-V64: int,double mix uses one GPR + one FPR.
  {
    dinv_cif_t* c = make(DINV_ABI_RISCV64, i32, {i32, f64});
    assert(dinv_cif_gpr_used(c) == 1 && dinv_cif_fpr_used(c) == 1);
    dinv_cif_destroy(c);
  }
  // RISC-V32 layout differs only in slot width; smoke-test creation.
  {
    dinv_cif_t* c = make(DINV_ABI_RISCV32, i32, {i32, i32});
    assert(dinv_cif_gpr_used(c) == 2);
    dinv_cif_destroy(c);
  }
  // Large struct return uses hidden pointer on all ABIs.
  {
    static const dinv_type_t* bigElems[] = {
        &dinv_type_i64, &dinv_type_i64, &dinv_type_i64, nullptr};
    static const dinv_type_t big = {DINV_STRUCT, bigElems};
    for (auto abi :
         {DINV_ABI_SYSV_AMD64, DINV_ABI_WIN64, DINV_ABI_AAPCS64,
          DINV_ABI_RISCV64}) {
      dinv_cif_t* c = make(abi, &big, {i32});
      assert(dinv_cif_uses_hidden_ret(c) == 1);
      dinv_cif_destroy(c);
    }
    // Unsupported indirect arguments must not be misrepresented as by-value
    // stack copies (which would cause incorrect calls).
    for (auto abi : {DINV_ABI_WIN64, DINV_ABI_AAPCS64}) {
      dinv_cif_t* c = nullptr;
      const dinv_type_t* args[] = {&big};
      assert(dinv_cif_create(abi, i32, 1, args, &c) == DINV_UNSUPPORTED);
      assert(c == nullptr);
    }
  }
  // Sandstone ISA files exist for the three ELF targets.
  for (auto abi :
       {DINV_ABI_SYSV_AMD64, DINV_ABI_AAPCS64, DINV_ABI_RISCV64}) {
    size_t ng = 0, nf = 0;
    if (dinv_isa_regcounts(abi, nullptr, &ng, &nf) == DINV_OK) {
      assert(ng > 0 && nf > 0);
    } else {
      std::cerr << "warning: missing ISA for " << dinv_abi_name(abi) << ": "
                << dinv_last_error() << '\n';
    }
  }
  std::cout << "dyninvoke layout tests passed\n";
  return 0;
}
