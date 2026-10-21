// dyninvoke.cpp - Tourchstone foreign-call implementation.
#include "dyninvoke.h"

#include <qBFD.hpp>

#include "simple_isa.hpp"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <deque>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifndef DINV_ISA_DIR
#define DINV_ISA_DIR ""
#endif

extern "C" {
void dinv_trampoline_sysv_amd64(void* func, const uint64_t* gpr,
                                const uint64_t* fpr, const uint8_t* stack,
                                size_t stack_bytes, const void* hidden,
                                uint64_t* out_gpr, uint64_t* out_fpr);
void dinv_trampoline_win64(void* func, const uint64_t* gpr,
                           const uint64_t* fpr, const uint8_t* stack,
                           size_t stack_bytes, const void* hidden,
                           uint64_t* out_gpr, uint64_t* out_fpr);
void dinv_trampoline_aapcs64(void* func, const uint64_t* gpr,
                             const uint64_t* fpr, const uint8_t* stack,
                             size_t stack_bytes, const void* hidden,
                             uint64_t* out_gpr, uint64_t* out_fpr);
void dinv_trampoline_riscv64(void* func, const uint64_t* gpr,
                             const uint64_t* fpr, const uint8_t* stack,
                             size_t stack_bytes, const void* hidden,
                             uint64_t* out_gpr, uint64_t* out_fpr);
void dinv_closure_dispatcher_sysv(void);
void dinv_closure_invoke_sysv(void* closure, const uint64_t* gpr,
                              const uint64_t* fpr, const uint8_t* stack,
                              uint64_t* out_gpr, uint64_t* out_fpr);
}

namespace {

thread_local std::string g_error;

dinv_status_t fail(dinv_status_t s, std::string m) {
  g_error = std::move(m);
  return s;
}
dinv_status_t ok() {
  g_error.clear();
  return DINV_OK;
}

size_t alignUp(size_t n, size_t a) {
  if (a == 0) return n;
  return (n + a - 1) & ~(a - 1);
}

bool isIntKind(dinv_kind_t k) {
  return k == DINV_BOOL || k == DINV_I8 || k == DINV_U8 || k == DINV_I16 ||
         k == DINV_U16 || k == DINV_I32 || k == DINV_U32 || k == DINV_I64 ||
         k == DINV_U64 || k == DINV_PTR;
}
bool isFloatKind(dinv_kind_t k) { return k == DINV_F32 || k == DINV_F64; }

size_t scalarSize(dinv_kind_t k) {
  switch (k) {
    case DINV_VOID: return 0;
    case DINV_BOOL:
    case DINV_I8:
    case DINV_U8: return 1;
    case DINV_I16:
    case DINV_U16: return 2;
    case DINV_I32:
    case DINV_U32:
    case DINV_F32: return 4;
    case DINV_I64:
    case DINV_U64:
    case DINV_F64:
    case DINV_PTR: return 8;
    default: return 0;
  }
}
size_t scalarAlign(dinv_kind_t k) {
  size_t s = scalarSize(k);
  if (k == DINV_BOOL) return 1;
  return s ? s : 1;
}

struct Leaf {
  size_t offset = 0;
  size_t size = 0;
  dinv_kind_t kind = DINV_VOID;
};

bool typeSizeAlignRec(const dinv_type_t* t, size_t& size, size_t& align,
                      std::vector<const dinv_type_t*>& stack) {
  if (!t) return false;
  if (stack.size() >= 64 || t->kind < DINV_VOID || t->kind > DINV_STRUCT)
    return false;
  if (t->kind != DINV_STRUCT) {
    size = scalarSize(t->kind);
    align = scalarAlign(t->kind);
    return true;
  }
  for (auto p : stack)
    if (p == t) return false;  // cyclic struct by value
  stack.push_back(t);
  size_t off = 0, mal = 1;
  if (!t->elements) {
    stack.pop_back();
    return false;
  }
  for (size_t i = 0; t->elements[i]; ++i) {
    size_t fs = 0, fa = 1;
    if (!typeSizeAlignRec(t->elements[i], fs, fa, stack)) {
      stack.pop_back();
      return false;
    }
    if (t->elements[i]->kind == DINV_VOID || fs == 0 ||
        off > std::numeric_limits<size_t>::max() - (fa - 1) - fs) {
      stack.pop_back();
      return false;
    }
    off = alignUp(off, fa);
    off += fs;
    if (fa > mal) mal = fa;
  }
  if (mal == 0) mal = 1;
  if (off > std::numeric_limits<size_t>::max() - (mal - 1)) {
    stack.pop_back();
    return false;
  }
  size = alignUp(off, mal);
  align = mal;
  stack.pop_back();
  return true;
}

bool collectLeavesRec(const dinv_type_t* t, size_t base,
                      std::vector<Leaf>& out,
                      std::vector<const dinv_type_t*>& stack) {
  if (!t) return false;
  if (t->kind != DINV_STRUCT) {
    out.push_back({base, scalarSize(t->kind), t->kind});
    return true;
  }
  for (auto p : stack)
    if (p == t) return false;
  stack.push_back(t);
  size_t off = base;
  // Need per-field offsets: recompute layout.
  for (size_t i = 0; t->elements && t->elements[i]; ++i) {
    size_t fs = 0, fa = 1;
    std::vector<const dinv_type_t*> s2(stack.begin(), stack.end());
    if (!typeSizeAlignRec(t->elements[i], fs, fa, s2)) {
      stack.pop_back();
      return false;
    }
    off = alignUp(off, fa);
    if (!collectLeavesRec(t->elements[i], off, out, stack)) {
      stack.pop_back();
      return false;
    }
    off += fs;
  }
  stack.pop_back();
  return true;
}

size_t typeSize(const dinv_type_t* t) {
  if (!t) return 0;
  size_t s = 0, a = 1;
  std::vector<const dinv_type_t*> st;
  if (!typeSizeAlignRec(t, s, a, st)) return 0;
  return s;
}
size_t typeAlign(const dinv_type_t* t) {
  if (!t) return 1;
  size_t s = 0, a = 1;
  std::vector<const dinv_type_t*> st;
  if (!typeSizeAlignRec(t, s, a, st)) return 1;
  return a ? a : 1;
}

// ---- layout ----
// Stable, owned descriptors: a cif and its closures survive the caller's
// temporary type arrays. Shared storage makes closure layout copies cheap.
struct TypeStorage {
  std::deque<dinv_type_t> types;
  std::deque<std::vector<const dinv_type_t*>> elements;
  const dinv_type_t* copy(const dinv_type_t* source) {
    types.push_back({source->kind, nullptr});
    auto* result = &types.back();
    if (source->kind == DINV_STRUCT) {
      elements.emplace_back();
      auto& fields = elements.back();
      for (size_t i = 0; source->elements[i]; ++i)
        fields.push_back(copy(source->elements[i]));
      fields.push_back(nullptr);
      result->elements = fields.data();
    }
    return result;
  }
};

struct CifLayout {
  std::shared_ptr<TypeStorage> types;
  dinv_abi_t abi = DINV_ABI_AUTO;
  const dinv_type_t* ret = nullptr;
  std::vector<const dinv_type_t*> args;
  bool hiddenRet = false;
  int gprUsed = 0;
  int fprUsed = 0;
  size_t stackBytes = 0;
  std::vector<std::vector<dinv_slot_t>> slots;
  std::vector<dinv_slot_t> retSlots;
};

dinv_slot_t makeSlot(dinv_loc_kind_t w, int reg, size_t so, size_t sz,
                     size_t fo) {
  dinv_slot_t s{};
  s.where = w;
  s.reg = reg;
  s.stack_offset = so;
  s.size = sz;
  s.field_offset = fo;
  return s;
}

dinv_abi_t resolveAbi(dinv_abi_t a) {
  if (a != DINV_ABI_AUTO) return a;
#if defined(_WIN32) && (defined(__x86_64__) || defined(_M_X64))
  return DINV_ABI_WIN64;
#elif defined(__x86_64__) || defined(_M_X64)
  return DINV_ABI_SYSV_AMD64;
#elif defined(__aarch64__) || defined(_M_ARM64)
  return DINV_ABI_AAPCS64;
#elif defined(__riscv) && __riscv_xlen == 32
  return DINV_ABI_RISCV32;
#elif defined(__riscv)
  return DINV_ABI_RISCV64;
#else
  return DINV_ABI_AUTO;
#endif
}

bool hostCanExecute(dinv_abi_t abi) {
  abi = resolveAbi(abi);
#if defined(__x86_64__) && !defined(_WIN32)
  return abi == DINV_ABI_SYSV_AMD64;
#elif defined(__x86_64__) || defined(_M_X64)
  return abi == DINV_ABI_WIN64;
#elif defined(__aarch64__) || defined(_M_ARM64)
  return abi == DINV_ABI_AAPCS64;
#elif defined(__riscv) && __riscv_xlen == 64
  return abi == DINV_ABI_RISCV64;
#else
  return false;
#endif
}

// SysV eightbyte classification.
struct Eightbyte {
  bool hasInt = false;
  bool hasFp = false;
  bool empty = true;
};

bool sysvClassifyStruct(const dinv_type_t* t, size_t structSize,
                        std::vector<Eightbyte>& ebs) {
  size_t n = (structSize + 7) / 8;
  if (n == 0 || n > 2) return false;
  ebs.assign(n, {});
  std::vector<Leaf> leaves;
  std::vector<const dinv_type_t*> st;
  if (!collectLeavesRec(t, 0, leaves, st)) return false;
  for (size_t i = 0; i < n; ++i) {
    size_t lo = i * 8, hi = lo + 8;
    if (hi > structSize) hi = structSize;
    Eightbyte e;
    e.empty = true;
    for (auto& l : leaves) {
      size_t llo = l.offset, lhi = l.offset + l.size;
      if (lhi <= lo || llo >= hi) continue;
      e.empty = false;
      if (isIntKind(l.kind))
        e.hasInt = true;
      else if (isFloatKind(l.kind))
        e.hasFp = true;
      else
        e.hasInt = true;
    }
    if (e.empty) e.hasInt = true;  // padding-only tail
    ebs[i] = e;
  }
  return true;
}

dinv_status_t layoutSysv(CifLayout& L) {
  size_t retSize = typeSize(L.ret);
  bool retStruct = L.ret && L.ret->kind == DINV_STRUCT;
  if (L.ret && L.ret->kind == DINV_VOID) {
    L.hiddenRet = false;
  } else if (retStruct) {
    if (retSize == 0)
      return fail(DINV_LAYOUT_ERROR, "empty struct return");
    if (retSize > 16) {
      L.hiddenRet = true;
    } else {
      std::vector<Eightbyte> ebs;
      if (!sysvClassifyStruct(L.ret, retSize, ebs))
        return fail(DINV_LAYOUT_ERROR, "cannot classify struct return");
      L.hiddenRet = false;
      int gi = 0, fi = 0;
      for (size_t i = 0; i < ebs.size(); ++i) {
        size_t off = i * 8, sz = retSize - off > 8 ? 8 : retSize - off;
        if (ebs[i].hasInt && !ebs[i].hasFp)
          L.retSlots.push_back(makeSlot(DINV_LOC_GPR, gi++, 0, sz, off));
        else if (ebs[i].hasFp && !ebs[i].hasInt)
          L.retSlots.push_back(makeSlot(DINV_LOC_FPR, fi++, 0, sz, off));
        else
          L.retSlots.push_back(makeSlot(DINV_LOC_GPR, gi++, 0, sz, off));
      }
    }
  } else if (L.ret) {
    if (isIntKind(L.ret->kind))
      L.retSlots.push_back(makeSlot(DINV_LOC_GPR, 0, 0, typeSize(L.ret), 0));
    else if (isFloatKind(L.ret->kind))
      L.retSlots.push_back(makeSlot(DINV_LOC_FPR, 0, 0, typeSize(L.ret), 0));
  }
  int gpr = L.hiddenRet ? 1 : 0;
  int fpr = 0;
  size_t stack = 0;
  L.slots.assign(L.args.size(), {});
  for (size_t i = 0; i < L.args.size(); ++i) {
    const dinv_type_t* t = L.args[i];
    if (!t) return fail(DINV_LAYOUT_ERROR, "null argument type");
    if (t->kind == DINV_VOID)
      return fail(DINV_LAYOUT_ERROR, "void argument");
    if (t->kind == DINV_STRUCT) {
      size_t sz = typeSize(t);
      if (sz == 0) return fail(DINV_LAYOUT_ERROR, "empty struct argument");
      if (sz > 16) {
        size_t off = alignUp(stack, 8);
        L.slots[i].push_back(makeSlot(DINV_LOC_STACK, -1, off, sz, 0));
        stack = off + alignUp(sz, 8);
        continue;
      }
      std::vector<Eightbyte> ebs;
      if (!sysvClassifyStruct(t, sz, ebs))
        return fail(DINV_LAYOUT_ERROR, "cannot classify struct argument");
      int needG = 0, needF = 0;
      for (auto& e : ebs) {
        if (e.hasInt && !e.hasFp)
          needG++;
        else if (e.hasFp && !e.hasInt)
          needF++;
        else
          needG++;
      }
      if (gpr + needG <= 6 && fpr + needF <= 8) {
        int gi = gpr, fi = fpr;
        for (size_t k = 0; k < ebs.size(); ++k) {
          size_t off = k * 8, ssz = sz - off > 8 ? 8 : sz - off;
          if (ebs[k].hasInt && !ebs[k].hasFp)
            L.slots[i].push_back(makeSlot(DINV_LOC_GPR, gi++, 0, ssz, off));
          else if (ebs[k].hasFp && !ebs[k].hasInt)
            L.slots[i].push_back(makeSlot(DINV_LOC_FPR, fi++, 0, ssz, off));
          else
            L.slots[i].push_back(makeSlot(DINV_LOC_GPR, gi++, 0, ssz, off));
        }
        gpr += needG;
        fpr += needF;
      } else {
        size_t off = alignUp(stack, 8);
        L.slots[i].push_back(makeSlot(DINV_LOC_STACK, -1, off, sz, 0));
        stack = off + alignUp(sz, 8);
      }
      continue;
    }
    if (isIntKind(t->kind)) {
      if (gpr < 6) {
        L.slots[i].push_back(makeSlot(DINV_LOC_GPR, gpr++, 0, 8, 0));
      } else {
        size_t off = alignUp(stack, 8);
        L.slots[i].push_back(makeSlot(DINV_LOC_STACK, -1, off, 8, 0));
        stack = off + 8;
      }
    } else if (isFloatKind(t->kind)) {
      if (fpr < 8) {
        L.slots[i].push_back(makeSlot(DINV_LOC_FPR, fpr++, 0, 8, 0));
      } else {
        size_t off = alignUp(stack, 8);
        L.slots[i].push_back(makeSlot(DINV_LOC_STACK, -1, off, 8, 0));
        stack = off + 8;
      }
    } else {
      return fail(DINV_LAYOUT_ERROR, "unknown argument kind");
    }
  }
  L.gprUsed = gpr;
  L.fprUsed = fpr;
  L.stackBytes = stack;
  return ok();
}

dinv_status_t layoutWin64(CifLayout& L) {
  size_t retSize = typeSize(L.ret);
  bool retStruct = L.ret && L.ret->kind == DINV_STRUCT;
  if (L.ret && L.ret->kind == DINV_VOID) {
    L.hiddenRet = false;
  } else if (retStruct) {
    if (retSize == 0) return fail(DINV_LAYOUT_ERROR, "empty struct return");
    if (retSize == 1 || retSize == 2 || retSize == 4 || retSize == 8) {
      L.hiddenRet = false;
      L.retSlots.push_back(makeSlot(DINV_LOC_GPR, 0, 0, retSize, 0));
    } else {
      L.hiddenRet = true;
    }
  } else if (L.ret) {
    if (isIntKind(L.ret->kind))
      L.retSlots.push_back(makeSlot(DINV_LOC_GPR, 0, 0, typeSize(L.ret), 0));
    else
      L.retSlots.push_back(makeSlot(DINV_LOC_FPR, 0, 0, typeSize(L.ret), 0));
  }
  int slot = L.hiddenRet ? 1 : 0;
  int gpr = L.hiddenRet ? 1 : 0, fpr = 0;
  size_t stack = 0;
  L.slots.assign(L.args.size(), {});
  for (size_t i = 0; i < L.args.size(); ++i) {
    const dinv_type_t* t = L.args[i];
    if (!t || t->kind == DINV_VOID)
      return fail(DINV_LAYOUT_ERROR, "invalid argument type");
    bool isStruct = t->kind == DINV_STRUCT;
    size_t sz = typeSize(t);
    if (isStruct && sz == 0)
      return fail(DINV_LAYOUT_ERROR, "empty struct argument");
    if (isStruct && sz != 1 && sz != 2 && sz != 4 && sz != 8)
      return fail(DINV_UNSUPPORTED, "Win64 indirect aggregate arguments are not implemented");
    if (slot < 4) {
      if (isFloatKind(t->kind)) {
        L.slots[i].push_back(makeSlot(DINV_LOC_FPR, slot, 0, 8, 0));
        fpr++;
      } else {
        L.slots[i].push_back(makeSlot(DINV_LOC_GPR, slot, 0, 8, 0));
        gpr++;
      }
      slot++;
    } else {
      size_t off = alignUp(stack, 8);
      L.slots[i].push_back(makeSlot(DINV_LOC_STACK, -1, off, 8, 0));
      stack = off + 8;
      slot++;
    }
  }
  L.gprUsed = gpr;
  L.fprUsed = fpr;
  L.stackBytes = stack;
  return ok();
}

bool aapcsIsHfa(const dinv_type_t* t, dinv_kind_t& elemKind, size_t& count,
                size_t& total) {
  elemKind = DINV_VOID;
  count = 0;
  total = typeSize(t);
  if (!t || t->kind != DINV_STRUCT || !t->elements) return false;
  for (size_t i = 0; t->elements[i]; ++i) {
    const dinv_type_t* f = t->elements[i];
    if (!f || f->kind == DINV_STRUCT) return false;  // only flat HFAs
    if (!isFloatKind(f->kind)) return false;
    if (elemKind == DINV_VOID)
      elemKind = f->kind;
    else if (elemKind != f->kind)
      return false;
    count++;
  }
  if (count == 0 || count > 4) return false;
  return true;
}

dinv_status_t layoutAapcs64(CifLayout& L) {
  size_t retSize = typeSize(L.ret);
  if (L.ret && L.ret->kind == DINV_VOID) {
    L.hiddenRet = false;
  } else if (L.ret && L.ret->kind == DINV_STRUCT) {
    dinv_kind_t ek;
    size_t c = 0, tot = 0;
    if (aapcsIsHfa(L.ret, ek, c, tot)) {
      L.hiddenRet = false;
      size_t esz = ek == DINV_F32 ? 4 : 8;
      for (size_t k = 0; k < c; ++k)
        L.retSlots.push_back(makeSlot(DINV_LOC_FPR, (int)k, 0, esz, k * esz));
    } else if (retSize <= 16 && retSize > 0) {
      L.hiddenRet = false;
      if (retSize <= 8)
        L.retSlots.push_back(makeSlot(DINV_LOC_GPR, 0, 0, retSize, 0));
      else {
        L.retSlots.push_back(makeSlot(DINV_LOC_GPR, 0, 0, 8, 0));
        L.retSlots.push_back(makeSlot(DINV_LOC_GPR, 1, 0, retSize - 8, 8));
      }
    } else if (retSize == 0) {
      return fail(DINV_LAYOUT_ERROR, "empty struct return");
    } else {
      L.hiddenRet = true;
    }
  } else if (L.ret) {
    if (isIntKind(L.ret->kind))
      L.retSlots.push_back(makeSlot(DINV_LOC_GPR, 0, 0, typeSize(L.ret), 0));
    else
      L.retSlots.push_back(makeSlot(DINV_LOC_FPR, 0, 0, typeSize(L.ret), 0));
  }
  int gpr = 0, fpr = 0;
  size_t stack = 0;
  L.slots.assign(L.args.size(), {});
  for (size_t i = 0; i < L.args.size(); ++i) {
    const dinv_type_t* t = L.args[i];
    if (!t || t->kind == DINV_VOID)
      return fail(DINV_LAYOUT_ERROR, "invalid argument type");
    if (t->kind == DINV_STRUCT) {
      size_t sz = typeSize(t);
      if (sz == 0) return fail(DINV_LAYOUT_ERROR, "empty struct argument");
      dinv_kind_t ek;
      size_t c = 0, tot = 0;
      if (aapcsIsHfa(t, ek, c, tot)) {
        size_t esz = ek == DINV_F32 ? 4 : 8;
        if (fpr + (int)c <= 8) {
          for (size_t k = 0; k < c; ++k)
            L.slots[i].push_back(
                makeSlot(DINV_LOC_FPR, fpr + (int)k, 0, esz, k * esz));
          fpr += (int)c;
          continue;
        }
        fpr = 8; // Once an HFA spills, following FP arguments also spill.
        size_t off = alignUp(stack, 8);
        L.slots[i].push_back(makeSlot(DINV_LOC_STACK, -1, off, sz, 0));
        stack = off + alignUp(sz, 8);
        continue;
      }
      if (sz > 16)
        return fail(DINV_UNSUPPORTED, "AAPCS64 indirect aggregate arguments are not implemented");
      if (sz <= 16) {
        int need = sz <= 8 ? 1 : 2;
        if (gpr + need <= 8) {
          if (need == 1)
            L.slots[i].push_back(makeSlot(DINV_LOC_GPR, gpr, 0, sz, 0));
          else {
            L.slots[i].push_back(makeSlot(DINV_LOC_GPR, gpr, 0, 8, 0));
            L.slots[i].push_back(
                makeSlot(DINV_LOC_GPR, gpr + 1, 0, sz - 8, 8));
          }
          gpr += need;
          continue;
        }
      }
      gpr = 8;
      size_t off = alignUp(stack, 8);
      L.slots[i].push_back(makeSlot(DINV_LOC_STACK, -1, off, sz, 0));
      stack = off + alignUp(sz, 8);
      continue;
    }
    if (isIntKind(t->kind)) {
      if (gpr < 8) {
        L.slots[i].push_back(makeSlot(DINV_LOC_GPR, gpr++, 0, 8, 0));
      } else {
        size_t off = alignUp(stack, 8);
        L.slots[i].push_back(makeSlot(DINV_LOC_STACK, -1, off, typeSize(t), 0));
        stack = off + 8;
      }
    } else {
      if (fpr < 8) {
        L.slots[i].push_back(makeSlot(DINV_LOC_FPR, fpr++, 0, 8, 0));
      } else {
        size_t off = alignUp(stack, 8);
        L.slots[i].push_back(makeSlot(DINV_LOC_STACK, -1, off, typeSize(t), 0));
        stack = off + 8;
      }
    }
  }
  L.gprUsed = gpr;
  L.fprUsed = fpr;
  L.stackBytes = alignUp(stack, 8);
  return ok();
}

dinv_status_t layoutRiscv(CifLayout& L, size_t xlen) {
  size_t two = 2 * xlen;
  auto structFitsRegs = [&](const dinv_type_t* t, size_t sz, int gpr,
                            int fpr) -> bool {
    std::vector<Leaf> leaves;
    std::vector<const dinv_type_t*> st;
    if (!collectLeavesRec(t, 0, leaves, st)) return false;
    int needG = 0, needF = 0;
    bool hasInt = false, hasFp = false;
    for (auto& l : leaves) {
      if (isIntKind(l.kind)) hasInt = true;
      if (isFloatKind(l.kind)) hasFp = true;
    }
    if (!hasFp) {
      if (sz > two) return false;
      needG = (int)((sz + xlen - 1) / xlen);
    } else if (!hasInt) {
      if (leaves.size() > 2 || sz > two) return false;
      needF = (int)leaves.size();
    } else {
      if (leaves.size() != 2 || sz > two) return false;
      for (auto& l : leaves) {
        if (isIntKind(l.kind))
          needG++;
        else
          needF++;
      }
    }
    return gpr + needG <= 8 && fpr + needF <= 8;
  };
  size_t retSize = typeSize(L.ret);
  if (L.ret && L.ret->kind == DINV_VOID) {
    L.hiddenRet = false;
  } else if (L.ret && L.ret->kind == DINV_STRUCT) {
    if (retSize == 0) return fail(DINV_LAYOUT_ERROR, "empty struct return");
    if (structFitsRegs(L.ret, retSize, 0, 0)) {
      L.hiddenRet = false;
      std::vector<Leaf> leaves;
      std::vector<const dinv_type_t*> st;
      collectLeavesRec(L.ret, 0, leaves, st);
      bool hasInt = false, hasFp = false;
      for (auto& l : leaves) {
        if (isIntKind(l.kind)) hasInt = true;
        if (isFloatKind(l.kind)) hasFp = true;
      }
      if (!hasFp) {
        if (retSize <= xlen)
          L.retSlots.push_back(makeSlot(DINV_LOC_GPR, 0, 0, retSize, 0));
        else {
          L.retSlots.push_back(makeSlot(DINV_LOC_GPR, 0, 0, xlen, 0));
          L.retSlots.push_back(
              makeSlot(DINV_LOC_GPR, 1, 0, retSize - xlen, xlen));
        }
      } else if (!hasInt) {
        for (size_t k = 0; k < leaves.size(); ++k)
          L.retSlots.push_back(makeSlot(DINV_LOC_FPR, (int)k, 0,
                                        leaves[k].size, leaves[k].offset));
      } else {
        int gi = 0, fi = 0;
        for (auto& l : leaves) {
          if (isIntKind(l.kind))
            L.retSlots.push_back(
                makeSlot(DINV_LOC_GPR, gi++, 0, l.size, l.offset));
          else
            L.retSlots.push_back(
                makeSlot(DINV_LOC_FPR, fi++, 0, l.size, l.offset));
        }
      }
    } else {
      L.hiddenRet = true;
    }
  } else if (L.ret) {
    if (isIntKind(L.ret->kind))
      L.retSlots.push_back(makeSlot(DINV_LOC_GPR, 0, 0, typeSize(L.ret), 0));
    else
      L.retSlots.push_back(makeSlot(DINV_LOC_FPR, 0, 0, typeSize(L.ret), 0));
  }
  int gpr = L.hiddenRet ? 1 : 0;
  int fpr = 0;
  size_t stack = 0;
  L.slots.assign(L.args.size(), {});
  for (size_t i = 0; i < L.args.size(); ++i) {
    const dinv_type_t* t = L.args[i];
    if (!t || t->kind == DINV_VOID)
      return fail(DINV_LAYOUT_ERROR, "invalid argument type");
    if (t->kind == DINV_STRUCT) {
      size_t sz = typeSize(t);
      if (sz == 0) return fail(DINV_LAYOUT_ERROR, "empty struct argument");
      if (structFitsRegs(t, sz, gpr, fpr)) {
        std::vector<Leaf> leaves;
        std::vector<const dinv_type_t*> st;
        collectLeavesRec(t, 0, leaves, st);
        bool hasInt = false, hasFp = false;
        for (auto& l : leaves) {
          if (isIntKind(l.kind)) hasInt = true;
          if (isFloatKind(l.kind)) hasFp = true;
        }
        if (!hasFp) {
          if (sz <= xlen) {
            L.slots[i].push_back(makeSlot(DINV_LOC_GPR, gpr++, 0, sz, 0));
          } else {
            L.slots[i].push_back(makeSlot(DINV_LOC_GPR, gpr++, 0, xlen, 0));
            L.slots[i].push_back(makeSlot(DINV_LOC_GPR, gpr++, 0, sz - xlen,
                                          xlen));
          }
        } else if (!hasInt) {
          for (auto& l : leaves)
            L.slots[i].push_back(
                makeSlot(DINV_LOC_FPR, fpr++, 0, l.size, l.offset));
        } else {
          for (auto& l : leaves) {
            if (isIntKind(l.kind))
              L.slots[i].push_back(
                  makeSlot(DINV_LOC_GPR, gpr++, 0, l.size, l.offset));
            else
              L.slots[i].push_back(
                  makeSlot(DINV_LOC_FPR, fpr++, 0, l.size, l.offset));
          }
        }
        continue;
      }
      size_t al = typeAlign(t);
      if (al > xlen) al = xlen;
      size_t off = alignUp(stack, al);
      L.slots[i].push_back(makeSlot(DINV_LOC_STACK, -1, off, sz, 0));
      stack = off + alignUp(sz, al);
      continue;
    }
    if (isIntKind(t->kind)) {
      if (gpr < 8) {
        L.slots[i].push_back(makeSlot(DINV_LOC_GPR, gpr++, 0, xlen, 0));
      } else {
        size_t al = typeAlign(t);
        if (al > xlen) al = xlen;
        size_t off = alignUp(stack, al);
        L.slots[i].push_back(makeSlot(DINV_LOC_STACK, -1, off, typeSize(t), 0));
        stack = off + alignUp(typeSize(t), al);
      }
    } else {
      if (fpr < 8) {
        L.slots[i].push_back(makeSlot(DINV_LOC_FPR, fpr++, 0, 8, 0));
      } else if (gpr < 8) {
        L.slots[i].push_back(makeSlot(DINV_LOC_GPR, gpr++, 0, xlen, 0));
      } else {
        size_t al = typeAlign(t);
        if (al > xlen) al = xlen;
        size_t off = alignUp(stack, al);
        L.slots[i].push_back(makeSlot(DINV_LOC_STACK, -1, off, typeSize(t), 0));
        stack = off + alignUp(typeSize(t), al);
      }
    }
  }
  L.gprUsed = gpr;
  L.fprUsed = fpr;
  L.stackBytes = alignUp(stack, 16);
  return ok();
}

dinv_status_t buildLayout(CifLayout& L) {
  switch (L.abi) {
    case DINV_ABI_SYSV_AMD64: return layoutSysv(L);
    case DINV_ABI_WIN64: return layoutWin64(L);
    case DINV_ABI_AAPCS64: return layoutAapcs64(L);
    case DINV_ABI_RISCV64: return layoutRiscv(L, 8);
    case DINV_ABI_RISCV32: return layoutRiscv(L, 4);
    default: return fail(DINV_UNSUPPORTED, "unknown ABI");
  }
}

}  // namespace

struct dinv_cif {
  CifLayout layout;
};

struct dinv_closure {
  dinv_cif cif;
  dinv_closure_fn_t fn = nullptr;
  void* user = nullptr;
  void* code = nullptr;
  size_t codeSize = 0;
#if defined(_WIN32)
  void* allocBase = nullptr;
#else
  void* allocBase = nullptr;
  size_t allocSize = 0;
#endif
};

static dinv_abi_t qabiToDinv(qbfd::abi::CallConv c) {
  using CC = qbfd::abi::CallConv;
  switch (c) {
    case CC::SysV_AMD64: return DINV_ABI_SYSV_AMD64;
    case CC::Win64: return DINV_ABI_WIN64;
    case CC::Aapcs64: return DINV_ABI_AAPCS64;
    case CC::Riscv64: return DINV_ABI_RISCV64;
    case CC::Riscv32: return DINV_ABI_RISCV32;
    default: return DINV_ABI_AUTO;
  }
}

// ---- marshalling ----
namespace {
void packArgs(const CifLayout& L, void** args, uint64_t* gpr, uint64_t* fpr,
              uint8_t* stack) {
  for (int i = 0; i < 8; ++i) gpr[i] = 0;
  for (int i = 0; i < 8; ++i) fpr[i] = 0;
  if (L.stackBytes && stack) memset(stack, 0, L.stackBytes);
  for (size_t i = 0; i < L.args.size(); ++i) {
    const uint8_t* src = (const uint8_t*)args[i];
    for (auto& s : L.slots[i]) {
      if (s.where == DINV_LOC_GPR) {
        uint64_t v = 0;
        size_t n = s.size > 8 ? 8 : s.size;
        // For scalar int slots the layout uses size 8/xlen; copy min(type,8).
        if (L.args[i]->kind != DINV_STRUCT) {
          n = typeSize(L.args[i]);
          if (n > 8) n = 8;
        }
        memcpy(&v, src + s.field_offset, n);
        gpr[s.reg] = v;
        // For RISC-V 32-bit GPR slots with size xlen=4, upper 4 must be zero;
        // memcpy already zero-extends.
      } else if (s.where == DINV_LOC_FPR) {
        uint64_t v = 0;
        size_t n = s.size;
        if (L.args[i]->kind != DINV_STRUCT) {
          n = typeSize(L.args[i]);
        }
        if (n > 8) n = 8;
        memcpy(&v, src + s.field_offset, n);
        fpr[s.reg] = v;
      } else if (s.where == DINV_LOC_STACK) {
        size_t n = s.size;
        if (L.args[i]->kind != DINV_STRUCT) {
          // Scalar stack slots: layout size is 8 (or xlen); copy type bytes.
          n = typeSize(L.args[i]);
        }
        memcpy(stack + s.stack_offset, src + s.field_offset, n);
      }
    }
  }
}

void unpackReturn(const CifLayout& L, const uint64_t* gpr, const uint64_t* fpr,
                  void* ret) {
  if (!ret) return;
  if (L.ret->kind == DINV_VOID) return;
  if (L.hiddenRet) return;  // already in hidden memory
  for (auto& s : L.retSlots) {
    uint64_t v = 0;
    if (s.where == DINV_LOC_GPR)
      v = gpr[s.reg];
    else if (s.where == DINV_LOC_FPR)
      v = fpr[s.reg];
    else
      continue;
    memcpy((uint8_t*)ret + s.field_offset, &v, s.size);
  }
}

// Closure unpack: build arg buffers from incoming regs/stack.
dinv_status_t closureUnpackAndCall(dinv_closure* c, const uint64_t* gpr,
                                  const uint64_t* fpr, const uint8_t* stack,
                                  uint64_t* outGpr, uint64_t* outFpr) {
  const CifLayout& L = c->cif.layout;
  std::vector<std::vector<uint8_t>> bufs(L.args.size());
  std::vector<void*> argv(L.args.size());
  for (size_t i = 0; i < L.args.size(); ++i) {
    size_t sz = typeSize(L.args[i]);
    bufs[i].assign(sz ? sz : 1, 0);
    argv[i] = bufs[i].data();
    for (auto& s : L.slots[i]) {
      uint64_t v = 0;
      if (s.where == DINV_LOC_GPR)
        v = gpr[s.reg];
      else if (s.where == DINV_LOC_FPR)
        v = fpr[s.reg];
      else {
        size_t n = s.size;
        if (L.args[i]->kind != DINV_STRUCT) n = typeSize(L.args[i]);
        if (n > s.size && L.args[i]->kind == DINV_STRUCT) n = s.size;
        memcpy(bufs[i].data() + s.field_offset, stack + s.stack_offset, n);
        continue;
      }
      size_t n = s.size;
      if (L.args[i]->kind != DINV_STRUCT) n = typeSize(L.args[i]);
      if (n > 8) n = 8;
      memcpy(bufs[i].data() + s.field_offset, &v, n);
    }
  }
  size_t retSize = typeSize(L.ret);
  std::vector<uint8_t> retBuf(retSize ? retSize : 1, 0);
  void* retPtr = retSize ? (void*)retBuf.data() : nullptr;
  if (L.hiddenRet) {
    retPtr = (void*)(uintptr_t)gpr[0];
  }
  c->fn(&c->cif, retPtr, argv.data(), c->user);
  for (int i = 0; i < 2; ++i) outGpr[i] = 0;
  for (int i = 0; i < 4; ++i) outFpr[i] = 0;
  if (L.hiddenRet) {
    outGpr[0] = gpr[0];  // SysV sret returns the destination in RAX.
    return ok();
  }
  if (!retSize || L.ret->kind == DINV_VOID) return ok();
  for (auto& s : L.retSlots) {
    uint64_t v = 0;
    memcpy(&v, retBuf.data() + s.field_offset, s.size);
    if (s.where == DINV_LOC_GPR)
      outGpr[s.reg] = v;
    else if (s.where == DINV_LOC_FPR)
      outFpr[s.reg] = v;
  }
  return ok();
}

}  // namespace

extern "C" void dinv_closure_invoke_sysv(void* closure, const uint64_t* gpr,
                                         const uint64_t* fpr,
                                         const uint8_t* stack,
                                         uint64_t* outGpr, uint64_t* outFpr) {
  auto* c = (dinv_closure*)closure;
  (void)closureUnpackAndCall(c, gpr, fpr, stack, outGpr, outFpr);
}

extern "C" {

const dinv_type_t dinv_type_void = {DINV_VOID, nullptr};
const dinv_type_t dinv_type_bool = {DINV_BOOL, nullptr};
const dinv_type_t dinv_type_i8 = {DINV_I8, nullptr};
const dinv_type_t dinv_type_u8 = {DINV_U8, nullptr};
const dinv_type_t dinv_type_i16 = {DINV_I16, nullptr};
const dinv_type_t dinv_type_u16 = {DINV_U16, nullptr};
const dinv_type_t dinv_type_i32 = {DINV_I32, nullptr};
const dinv_type_t dinv_type_u32 = {DINV_U32, nullptr};
const dinv_type_t dinv_type_i64 = {DINV_I64, nullptr};
const dinv_type_t dinv_type_u64 = {DINV_U64, nullptr};
const dinv_type_t dinv_type_f32 = {DINV_F32, nullptr};
const dinv_type_t dinv_type_f64 = {DINV_F64, nullptr};
const dinv_type_t dinv_type_ptr = {DINV_PTR, nullptr};

const char* dinv_version(void) { return "0.1.0"; }

const char* dinv_status_string(dinv_status_t s) {
  switch (s) {
    case DINV_OK: return "ok";
    case DINV_ERROR: return "error";
    case DINV_INVALID_ARGUMENT: return "invalid argument";
    case DINV_UNSUPPORTED: return "unsupported";
    case DINV_NO_MEMORY: return "out of memory";
    case DINV_LAYOUT_ERROR: return "layout error";
    case DINV_CALL_ERROR: return "call error";
    case DINV_NOT_FOUND: return "not found";
    default: return "unknown";
  }
}

const char* dinv_last_error(void) { return g_error.c_str(); }

const char* dinv_abi_name(dinv_abi_t abi) {
  switch (abi) {
    case DINV_ABI_AUTO: return "auto";
    case DINV_ABI_SYSV_AMD64: return "sysv_amd64";
    case DINV_ABI_WIN64: return "win64";
    case DINV_ABI_AAPCS64: return "aapcs64";
    case DINV_ABI_RISCV64: return "riscv64";
    case DINV_ABI_RISCV32: return "riscv32";
    default: return "unknown";
  }
}

dinv_status_t dinv_abi_for_name(const char* name, dinv_abi_t* out) {
  if (!name || !out) return fail(DINV_INVALID_ARGUMENT, "name and out required");
  std::string n(name);
  for (auto& ch : n) ch = (char)tolower((unsigned char)ch);
  if (n == "auto") {
    *out = DINV_ABI_AUTO;
    return ok();
  }
  if (n == "sysv_amd64" || n == "sysv" || n == "amd64_sysv" || n == "elf_amd64") {
    *out = DINV_ABI_SYSV_AMD64;
    return ok();
  }
  if (n == "win64" || n == "windows_x64" || n == "pe_amd64") {
    *out = DINV_ABI_WIN64;
    return ok();
  }
  if (n == "aapcs64" || n == "aarch64" || n == "arm64") {
    *out = DINV_ABI_AAPCS64;
    return ok();
  }
  if (n == "riscv64" || n == "riscv64_lp64d") {
    *out = DINV_ABI_RISCV64;
    return ok();
  }
  if (n == "riscv32" || n == "riscv32_ilp32d") {
    *out = DINV_ABI_RISCV32;
    return ok();
  }
  return fail(DINV_NOT_FOUND, "unknown ABI name");
}

dinv_abi_t dinv_abi_host(void) { return resolveAbi(DINV_ABI_AUTO); }
int dinv_abi_can_execute(dinv_abi_t abi) { return hostCanExecute(abi) ? 1 : 0; }

const char* dinv_abi_isa_arch(dinv_abi_t abi) {
  switch (resolveAbi(abi)) {
    case DINV_ABI_SYSV_AMD64:
    case DINV_ABI_WIN64: return "amd64";
    case DINV_ABI_AAPCS64: return "aarch64";
    case DINV_ABI_RISCV64: return "riscv64";
    case DINV_ABI_RISCV32: return "riscv32";
    default: return "";
  }
}

const char* dinv_abi_isa_file(dinv_abi_t abi) {
  switch (resolveAbi(abi)) {
    case DINV_ABI_SYSV_AMD64:
    case DINV_ABI_WIN64: return "x86.isa";
    case DINV_ABI_AAPCS64: return "aarch64.isa";
    case DINV_ABI_RISCV64: return "riscv64.isa";
    case DINV_ABI_RISCV32: return "riscv32.isa";
    default: return "";
  }
}

const char* dinv_kind_name(dinv_kind_t k) {
  switch (k) {
    case DINV_VOID: return "void";
    case DINV_BOOL: return "bool";
    case DINV_I8: return "i8";
    case DINV_U8: return "u8";
    case DINV_I16: return "i16";
    case DINV_U16: return "u16";
    case DINV_I32: return "i32";
    case DINV_U32: return "u32";
    case DINV_I64: return "i64";
    case DINV_U64: return "u64";
    case DINV_F32: return "f32";
    case DINV_F64: return "f64";
    case DINV_PTR: return "ptr";
    case DINV_STRUCT: return "struct";
    default: return "unknown";
  }
}

dinv_status_t dinv_type_for_name(const char* name, const dinv_type_t** out) {
  if (!name || !out) return fail(DINV_INVALID_ARGUMENT, "name and out required");
  std::string n(name);
  for (auto& ch : n) ch = (char)tolower((unsigned char)ch);
  const dinv_type_t* t = nullptr;
  if (n == "void") t = &dinv_type_void;
  else if (n == "bool") t = &dinv_type_bool;
  else if (n == "i8" || n == "char" || n == "int8") t = &dinv_type_i8;
  else if (n == "u8" || n == "uint8") t = &dinv_type_u8;
  else if (n == "i16" || n == "short" || n == "int16") t = &dinv_type_i16;
  else if (n == "u16" || n == "uint16") t = &dinv_type_u16;
  else if (n == "i32" || n == "int" || n == "int32") t = &dinv_type_i32;
  else if (n == "u32" || n == "uint" || n == "uint32") t = &dinv_type_u32;
  else if (n == "i64" || n == "long" || n == "int64") t = &dinv_type_i64;
  else if (n == "u64" || n == "ulong" || n == "uint64") t = &dinv_type_u64;
  else if (n == "f32" || n == "float") t = &dinv_type_f32;
  else if (n == "f64" || n == "double") t = &dinv_type_f64;
  else if (n == "ptr" || n == "pointer" || n == "p" || n == "string") t = &dinv_type_ptr;
  else return fail(DINV_NOT_FOUND, "unknown type name");
  *out = t;
  return ok();
}

const dinv_type_t* dinv_type_named(const char* name) {
  const dinv_type_t* result = nullptr;
  (void)dinv_type_for_name(name, &result);
  return result;
}

size_t dinv_type_size(const dinv_type_t* t) { return typeSize(t); }
size_t dinv_type_align(const dinv_type_t* t) { return typeAlign(t); }

dinv_status_t dinv_cif_create(dinv_abi_t abi, const dinv_type_t* ret,
                              size_t nargs, const dinv_type_t* const* args,
                              dinv_cif_t** out) {
  if (!out) return fail(DINV_INVALID_ARGUMENT, "out required");
  *out = nullptr;
  if (!ret) return fail(DINV_INVALID_ARGUMENT, "return type required");
  if (nargs > 64) return fail(DINV_INVALID_ARGUMENT, "too many arguments");
  if (nargs && !args) return fail(DINV_INVALID_ARGUMENT, "args required");
  dinv_abi_t r = resolveAbi(abi);
  if (r == DINV_ABI_AUTO) return fail(DINV_UNSUPPORTED, "cannot resolve ABI");
  try {
    auto c = std::make_unique<dinv_cif>();
    c->layout.abi = r;
    c->layout.ret = ret;
    if (nargs) c->layout.args.assign(args, args + nargs);
    // Validate types.
    {
      size_t s = 0, a = 1;
      std::vector<const dinv_type_t*> st;
      if (!typeSizeAlignRec(ret, s, a, st))
        return fail(DINV_LAYOUT_ERROR, "invalid return type");
      for (size_t i = 0; i < nargs; ++i) {
        if (!c->layout.args[i])
          return fail(DINV_INVALID_ARGUMENT, "null argument type");
        if (c->layout.args[i]->kind == DINV_VOID)
          return fail(DINV_INVALID_ARGUMENT, "void argument");
        size_t ss = 0, aa = 1;
        std::vector<const dinv_type_t*> st2;
        if (!typeSizeAlignRec(c->layout.args[i], ss, aa, st2))
          return fail(DINV_LAYOUT_ERROR, "invalid struct argument");
        if (c->layout.args[i]->kind == DINV_STRUCT && ss == 0)
          return fail(DINV_LAYOUT_ERROR, "empty struct argument");
      }
      if (ret->kind == DINV_STRUCT) {
        size_t ss = typeSize(ret);
        if (ss == 0) return fail(DINV_LAYOUT_ERROR, "empty struct return");
      }
    }
    c->layout.types = std::make_shared<TypeStorage>();
    c->layout.ret = c->layout.types->copy(ret);
    for (auto& arg : c->layout.args) arg = c->layout.types->copy(arg);
    dinv_status_t rc = buildLayout(c->layout);
    if (rc != DINV_OK) {
      return rc;
    }
    *out = c.release();
    return ok();
  } catch (const std::bad_alloc&) {
    return fail(DINV_NO_MEMORY, "out of memory");
  } catch (const std::exception& e) {
    return fail(DINV_ERROR, e.what());
  }
}

void dinv_cif_destroy(dinv_cif_t* c) { delete c; }
dinv_abi_t dinv_cif_abi(const dinv_cif_t* c) {
  return c ? c->layout.abi : DINV_ABI_AUTO;
}
size_t dinv_cif_nargs(const dinv_cif_t* c) {
  return c ? c->layout.args.size() : 0;
}
int dinv_cif_gpr_used(const dinv_cif_t* c) {
  return c ? c->layout.gprUsed : 0;
}
int dinv_cif_fpr_used(const dinv_cif_t* c) {
  return c ? c->layout.fprUsed : 0;
}
size_t dinv_cif_stack_bytes(const dinv_cif_t* c) {
  return c ? c->layout.stackBytes : 0;
}
int dinv_cif_uses_hidden_ret(const dinv_cif_t* c) {
  return c && c->layout.hiddenRet ? 1 : 0;
}

dinv_status_t dinv_cif_arg_slots(const dinv_cif_t* c, size_t index,
                                 dinv_slot_t* slots, size_t* inout_count) {
  if (!c || !inout_count)
    return fail(DINV_INVALID_ARGUMENT, "cif and count required");
  if (index >= c->layout.slots.size())
    return fail(DINV_INVALID_ARGUMENT, "argument index out of range");
  auto& v = c->layout.slots[index];
  if (!slots) {
    *inout_count = v.size();
    return ok();
  }
  size_t n = *inout_count < v.size() ? *inout_count : v.size();
  for (size_t i = 0; i < n; ++i) slots[i] = v[i];
  *inout_count = v.size();
  return ok();
}

dinv_status_t dinv_call(dinv_cif_t* cif, void* func, void* ret, void** args) {
  if (!cif || !func) return fail(DINV_INVALID_ARGUMENT, "cif and func required");
  const CifLayout& L = cif->layout;
  if (L.args.size() && !args)
    return fail(DINV_INVALID_ARGUMENT, "args required");
  for (size_t i = 0; i < L.args.size(); ++i)
    if (!args[i]) return fail(DINV_INVALID_ARGUMENT, "null argument storage");
  if (!hostCanExecute(L.abi))
    return fail(DINV_UNSUPPORTED,
                "call ABI is not the host ABI; layout query only");
  try {
    size_t retSize = typeSize(L.ret);
    if (retSize && !ret)
      return fail(DINV_INVALID_ARGUMENT, "return buffer required");
    uint64_t gpr[8] = {0};
    uint64_t fpr[8] = {0};
    std::vector<uint8_t> stack(L.stackBytes, 0);
    const void* hidden = L.hiddenRet ? ret : nullptr;
    packArgs(L, args, gpr, fpr, stack.data());
    // AArch64's trampoline uses x8; other ABIs reserve the first GPR.
    if (L.hiddenRet && L.abi != DINV_ABI_AAPCS64)
      gpr[0] = (uint64_t)(uintptr_t)ret;
    uint64_t outGpr[2] = {0, 0};
    uint64_t outFpr[4] = {0, 0, 0, 0};
    const uint8_t* stackPtr = stack.empty() ? nullptr : stack.data();
    switch (L.abi) {
#if defined(__x86_64__) && !defined(_WIN32)
      case DINV_ABI_SYSV_AMD64:
        dinv_trampoline_sysv_amd64(func, gpr, fpr, stackPtr, L.stackBytes, hidden,
                                   outGpr, outFpr);
        break;
#endif
#if defined(__x86_64__) || defined(_M_X64)
      case DINV_ABI_WIN64:
        dinv_trampoline_win64(func, gpr, fpr, stackPtr, L.stackBytes, hidden,
                               outGpr, outFpr);
        break;
#endif
#if defined(__aarch64__) || defined(_M_ARM64)
      case DINV_ABI_AAPCS64:
        dinv_trampoline_aapcs64(func, gpr, fpr, stackPtr, L.stackBytes, hidden,
                                 outGpr, outFpr);
        break;
#endif
#if defined(__riscv) && __riscv_xlen == 64
      case DINV_ABI_RISCV64:
        dinv_trampoline_riscv64(func, gpr, fpr, stackPtr, L.stackBytes, hidden,
                                 outGpr, outFpr);
        break;
#endif
      default:
        return fail(DINV_UNSUPPORTED, "no trampoline for this host/ABI");
    }
    if (!L.hiddenRet && retSize)
      unpackReturn(L, outGpr, outFpr, ret);
    return ok();
  } catch (const std::bad_alloc&) {
    return fail(DINV_NO_MEMORY, "cannot allocate call storage");
  } catch (const std::exception& error) {
    return fail(DINV_CALL_ERROR, error.what());
  }
}

dinv_status_t dinv_call_simple(dinv_abi_t abi, const dinv_type_t* ret,
                               void* func, void* ret_out, size_t nargs,
                               const dinv_type_t* const* arg_types,
                               void** args) {
  dinv_cif_t* cif = nullptr;
  dinv_status_t rc = dinv_cif_create(abi, ret, nargs, arg_types, &cif);
  if (rc != DINV_OK) return rc;
  rc = dinv_call(cif, func, ret_out, args);
  dinv_cif_destroy(cif);
  return rc;
}

dinv_status_t dinv_closure_create(dinv_cif_t* cif, dinv_closure_fn_t fn,
                                  void* user, dinv_closure_t** out_closure,
                                  void** out_fn) {
  if (out_closure) *out_closure = nullptr;
  if (out_fn) *out_fn = nullptr;
  if (!cif || !fn || !out_closure || !out_fn)
    return fail(DINV_INVALID_ARGUMENT, "cif, fn and outputs required");
  if (cif->layout.abi != DINV_ABI_SYSV_AMD64)
    return fail(DINV_UNSUPPORTED, "closures only support sysv_amd64");
#if !defined(__x86_64__) || defined(_WIN32)
  (void)fn;
  (void)user;
  return fail(DINV_UNSUPPORTED, "closures need an x86_64 SysV host");
#else
  try {
    auto owner = std::make_unique<dinv_closure>();
    auto* c = owner.get();
    c->cif.layout = cif->layout;
    c->fn = fn;
    c->user = user;
#if defined(_WIN32)
    void* mem = VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE,
                             PAGE_EXECUTE_READWRITE);
    if (!mem) {
      return fail(DINV_NO_MEMORY, "VirtualAlloc failed");
    }
    c->allocBase = mem;
    c->code = mem;
    c->codeSize = 32;
#else
    long ps = sysconf(_SC_PAGESIZE);
    if (ps <= 0) ps = 4096;
    void* mem =
        mmap(nullptr, (size_t)ps, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
             -1, 0);
    if (mem == MAP_FAILED) {
      return fail(DINV_NO_MEMORY, "mmap failed");
    }
    c->allocBase = mem;
    c->allocSize = (size_t)ps;
    c->code = mem;
    c->codeSize = (size_t)ps;
#endif
    uint8_t* p = (uint8_t*)c->code;
    // movabs r10, closure
    p[0] = 0x49;
    p[1] = 0xBA;
    memcpy(p + 2, &c, 8);
    // movabs r11, dispatcher
    void* disp = (void*)dinv_closure_dispatcher_sysv;
    p[10] = 0x49;
    p[11] = 0xBB;
    memcpy(p + 12, &disp, 8);
    // jmp r11
    p[20] = 0x41;
    p[21] = 0xFF;
    p[22] = 0xE3;
    p[23] = 0x90;  // nop pad
#if !defined(_WIN32)
    __builtin___clear_cache((char*)c->code, (char*)c->code + 24);
    if (mprotect(c->allocBase, c->allocSize, PROT_READ | PROT_EXEC) != 0) {
      munmap(c->allocBase, c->allocSize);
      return fail(DINV_ERROR, "mprotect failed");
    }
#endif
    *out_closure = owner.release();
    *out_fn = c->code;
    return ok();
  } catch (const std::bad_alloc&) {
    return fail(DINV_NO_MEMORY, "out of memory");
  }
#endif
}

void dinv_closure_destroy(dinv_closure_t* c) {
  if (!c) return;
#if defined(_WIN32)
  if (c->allocBase) VirtualFree(c->allocBase, 0, MEM_RELEASE);
#else
  if (c->allocBase) munmap(c->allocBase, c->allocSize ? c->allocSize : 4096);
#endif
  delete c;
}

dinv_status_t dinv_abi_for_object(const char* path, dinv_abi_t* out_abi) {
  if (!path || !out_abi) return fail(DINV_INVALID_ARGUMENT, "path and out required");
  auto opened = qbfd::open(std::filesystem::path(path));
  if (!opened) return fail(DINV_NOT_FOUND, opened.error().message);
  auto conv = qbfd::abi::convFor(opened->object->format(), opened->object->arch());
  if (conv == qbfd::abi::CallConv::Unknown)
    return fail(DINV_UNSUPPORTED, "object arch/format has no call ABI");
  *out_abi = qabiToDinv(conv);
  return ok();
}

dinv_status_t dinv_inspect_object(const char* path, const char* symbol,
                                  dinv_abi_t* out_abi, int* out_found) {
  if (!path) return fail(DINV_INVALID_ARGUMENT, "path required");
  auto opened = qbfd::open(std::filesystem::path(path));
  if (!opened) return fail(DINV_NOT_FOUND, opened.error().message);
  auto conv = qbfd::abi::convFor(opened->object->format(), opened->object->arch());
  if (out_abi) {
    if (conv == qbfd::abi::CallConv::Unknown)
      *out_abi = DINV_ABI_AUTO;
    else
      *out_abi = qabiToDinv(conv);
  }
  if (out_found) {
    if (!symbol || !*symbol)
      *out_found = 1;
    else
      *out_found = opened->object->findSymbol(symbol) ? 1 : 0;
  }
  if (conv == qbfd::abi::CallConv::Unknown)
    return fail(DINV_UNSUPPORTED, "object arch/format has no call ABI");
  return ok();
}

const char* dinv_isa_default_dir(void) {
  const char* d = DINV_ISA_DIR;
  return (d && *d) ? d : "";
}

dinv_status_t dinv_isa_regcounts(dinv_abi_t abi, const char* isa_dir,
                                 size_t* out_gpr, size_t* out_fpr) {
  if (!out_gpr || !out_fpr)
    return fail(DINV_INVALID_ARGUMENT, "output counts required");
  dinv_abi_t r = resolveAbi(abi);
  const char* file = dinv_abi_isa_file(r);
  if (!file || !*file) return fail(DINV_UNSUPPORTED, "ABI has no ISA file");
  std::string dir = (isa_dir && *isa_dir) ? isa_dir : dinv_isa_default_dir();
  if (dir.empty()) return fail(DINV_NOT_FOUND, "ISA directory unknown");
  std::filesystem::path p = std::filesystem::path(dir) / file;
  try {
    auto isa = qarch::simple_isa::load_file(p);
    std::unordered_map<unsigned, bool> gprNums, fprNums;
    // unordered_map needs <unordered_map>; use std::map via includes? Add local.
    // Count distinct register numbers per class (aliases excluded by load).
    for (auto& kv : isa.registers) {
      auto& reg = kv.second;
      if (reg.regclass == "GPR")
        gprNums[reg.number] = true;
      else if (reg.regclass == "FPR")
        fprNums[reg.number] = true;
    }
    *out_gpr = gprNums.size();
    *out_fpr = fprNums.size();
    if (*out_gpr == 0 && *out_fpr == 0)
      return fail(DINV_NOT_FOUND, "ISA has no registers");
    return ok();
  } catch (const std::exception& e) {
    return fail(DINV_NOT_FOUND, std::string("cannot load ISA: ") + e.what());
  }
}

}  // extern "C"
