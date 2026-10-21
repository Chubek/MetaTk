// dinvoke.cpp - CLI over dynload (open) + dyninvoke (call) + qBFD (inspect).
#include "dyninvoke.h"
#include "dynload.h"

#include <options.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using qobj::OptionSpec;

namespace {
constexpr OptionSpec kHelp{'h', "help", false, "", "show this help"};
constexpr OptionSpec kAbi{0, "abi", true, "name", "call ABI (auto, sysv_amd64, win64, aapcs64, riscv64, riscv32)"};
constexpr OptionSpec kRet{0, "ret", true, "type", "return type (default i32)"};
constexpr OptionSpec kArg{0, "arg", true, "type:value", "argument (repeatable)"};
constexpr OptionSpec kLayout{0, "layout", false, "", "dump register/stack layout and exit"};
constexpr OptionSpec kInspect{'i', "inspect", false, "", "print qBFD ABI and symbol check"};
constexpr OptionSpec kIsaDir{0, "isa-dir", true, "dir", "sandstone ISA directory"};
constexpr OptionSpec kLibPath{'L', "library-path", true, "dir", "dynload search path"};

bool splitArg(const std::string& s, std::string& type, std::string& value) {
  auto p = s.find(':');
  if (p == std::string::npos) return false;
  type = s.substr(0, p);
  value = s.substr(p + 1);
  return !type.empty();
}

bool parseInt(const std::string& s, int64_t& out) {
  try {
    size_t u = 0;
    long long v = std::stoll(s, &u, 0);
    if (u != s.size()) return false;
    out = v;
    return true;
  } catch (...) {
    return false;
  }
}
bool parseUint(const std::string& s, uint64_t& out) {
  try {
    size_t u = 0;
    unsigned long long v = std::stoull(s, &u, 0);
    if (u != s.size()) return false;
    out = v;
    return true;
  } catch (...) {
    return false;
  }
}

const char* locName(dinv_loc_kind_t w) {
  switch (w) {
    case DINV_LOC_GPR: return "gpr";
    case DINV_LOC_FPR: return "fpr";
    case DINV_LOC_STACK: return "stack";
    default: return "void";
  }
}
}  // namespace

int main(int argc, char** argv) {
  qobj::Options opts("dinvoke", "[options] <library> <symbol>",
                     {kHelp, kAbi, kRet, kArg, kLayout, kInspect, kIsaDir, kLibPath});
  if (!opts.parse(argc, argv)) {
    std::cerr << opts.error() << '\n' << opts.usage();
    return 2;
  }
  if (opts.has(kHelp)) {
    std::cout << opts.usage();
    return 0;
  }
  auto pos = opts.positional();
  if (pos.size() < 2) {
    std::cerr << opts.usage();
    return 2;
  }
  std::string libPath = pos[0], symbol = pos[1];

  dinv_abi_t abi = DINV_ABI_AUTO;
  if (opts.has(kAbi)) {
    if (dinv_abi_for_name(opts.value(kAbi).c_str(), &abi) != DINV_OK) {
      std::cerr << "unknown ABI: " << opts.value(kAbi) << '\n';
      return 2;
    }
  }
  const dinv_type_t* retType = &dinv_type_i32;
  if (opts.has(kRet)) {
    if (dinv_type_for_name(opts.value(kRet).c_str(), &retType) != DINV_OK) {
      std::cerr << "unknown return type: " << opts.value(kRet) << '\n';
      return 2;
    }
  }
  struct ParsedArg {
    const dinv_type_t* type = nullptr;
    std::string value;
  };
  std::vector<ParsedArg> pargs;
  for (auto& a : opts.valueList(kArg)) {
    std::string t, v;
    if (!splitArg(a, t, v)) {
      std::cerr << "bad --arg (want type:value): " << a << '\n';
      return 2;
    }
    const dinv_type_t* ty = nullptr;
    if (dinv_type_for_name(t.c_str(), &ty) != DINV_OK) {
      std::cerr << "unknown arg type: " << t << '\n';
      return 2;
    }
    pargs.push_back({ty, v});
  }

  const char* isaDir = opts.has(kIsaDir) ? opts.value(kIsaDir).c_str() : nullptr;

  // qBFD inspect via dyninvoke (format/arch -> ABI + symbol presence).
  dinv_abi_t fileAbi = DINV_ABI_AUTO;
  int found = 0;
  dinv_status_t irc =
      dinv_inspect_object(libPath.c_str(), symbol.c_str(), &fileAbi, &found);
  if (opts.has(kInspect) || irc != DINV_OK) {
    std::cout << "file: " << libPath << '\n';
    if (irc == DINV_OK) {
      std::cout << "qbfd-abi: " << dinv_abi_name(fileAbi) << '\n';
      std::cout << "isa: " << dinv_abi_isa_arch(fileAbi) << " ("
                << dinv_abi_isa_file(fileAbi) << ")\n";
      std::cout << "symbol " << symbol << ": " << (found ? "found" : "missing")
                << '\n';
    } else {
      std::cout << "inspect: " << dinv_last_error() << '\n';
    }
    size_t ng = 0, nf = 0;
    dinv_abi_t checkAbi = (abi == DINV_ABI_AUTO && irc == DINV_OK) ? fileAbi : abi;
    if (checkAbi != DINV_ABI_AUTO &&
        dinv_isa_regcounts(checkAbi, isaDir, &ng, &nf) == DINV_OK)
      std::cout << "isa-regs: gpr=" << ng << " fpr=" << nf << '\n';
    else if (checkAbi != DINV_ABI_AUTO)
      std::cout << "isa-regs: unavailable (" << dinv_last_error() << ")\n";
    if (opts.has(kInspect) && !opts.has(kLayout) && pargs.empty()) return irc == DINV_OK ? 0 : 1;
    if (irc != DINV_OK && abi == DINV_ABI_AUTO) return 1;
  }
  if (abi == DINV_ABI_AUTO && irc == DINV_OK) abi = fileAbi;

  // Build cif.
  std::vector<const dinv_type_t*> argTypes;
  for (auto& p : pargs) argTypes.push_back(p.type);
  dinv_cif_t* cif = nullptr;
  if (dinv_cif_create(abi, retType, argTypes.size(), argTypes.data(), &cif) != DINV_OK) {
    std::cerr << "cif: " << dinv_last_error() << '\n';
    return 1;
  }

  if (opts.has(kLayout) || opts.has(kInspect)) {
    std::cout << "abi: " << dinv_abi_name(dinv_cif_abi(cif)) << '\n';
    std::cout << "gpr-used: " << dinv_cif_gpr_used(cif)
              << " fpr-used: " << dinv_cif_fpr_used(cif)
              << " stack-bytes: " << dinv_cif_stack_bytes(cif)
              << " hidden-ret: " << (dinv_cif_uses_hidden_ret(cif) ? "yes" : "no")
              << '\n';
    for (size_t i = 0; i < argTypes.size(); ++i) {
      size_t n = 0;
      dinv_cif_arg_slots(cif, i, nullptr, &n);
      std::vector<dinv_slot_t> slots(n);
      size_t m = n;
      dinv_cif_arg_slots(cif, i, slots.data(), &m);
      std::cout << "arg" << i << ":";
      for (auto& s : slots) {
        std::cout << " " << locName(s.where);
        if (s.where == DINV_LOC_GPR || s.where == DINV_LOC_FPR)
          std::cout << s.reg;
        else
          std::cout << "+" << s.stack_offset;
        std::cout << ":" << s.size;
      }
      std::cout << '\n';
    }
    if (opts.has(kLayout) && pargs.empty()) {
      dinv_cif_destroy(cif);
      return 0;
    }
  }

  // Open + resolve via dynload.
  tdyn_env_t* env = nullptr;
  if (tdyn_env_create(&env) != TDYN_OK) {
    std::cerr << "dynload env: " << tdyn_last_error() << '\n';
    dinv_cif_destroy(cif);
    return 1;
  }
  for (auto& d : opts.valueList(kLibPath)) tdyn_env_add_search_path(env, d.c_str());
  tdyn_library_t* lib = nullptr;
  uint32_t flags = TDYN_OPEN_NOW | TDYN_OPEN_LOCAL;
  if (tdyn_env_open(env, libPath.c_str(), flags, &lib) != TDYN_OK) {
    std::cerr << "open: " << tdyn_last_error() << '\n';
    tdyn_env_destroy(env);
    dinv_cif_destroy(cif);
    return 1;
  }
  void* fn = nullptr;
  if (tdyn_library_symbol(lib, symbol.c_str(), &fn) != TDYN_OK) {
    std::cerr << "resolve: " << tdyn_last_error() << '\n';
    tdyn_env_destroy(env);
    dinv_cif_destroy(cif);
    return 1;
  }

  // Marshal args.
  std::vector<uint64_t> ibuf(pargs.size(), 0);
  std::vector<double> fbuf(pargs.size(), 0.0);
  std::vector<std::string> sbuf(pargs.size());
  std::vector<void*> callArgs(pargs.size(), nullptr);
  for (size_t i = 0; i < pargs.size(); ++i) {
    auto* t = pargs[i].type;
    auto& v = pargs[i].value;
    if (t == &dinv_type_f32) {
      float f = strtof(v.c_str(), nullptr);
      memcpy(&fbuf[i], &f, 4);
      callArgs[i] = &fbuf[i];
    } else if (t == &dinv_type_f64) {
      double d = strtod(v.c_str(), nullptr);
      fbuf[i] = d;
      callArgs[i] = &fbuf[i];
    } else if (t == &dinv_type_ptr) {
      if (v.rfind("str:", 0) == 0) {
        sbuf[i] = v.substr(4);
        callArgs[i] = sbuf[i].data();
      } else {
        uint64_t p = 0;
        if (!parseUint(v, p)) {
          std::cerr << "bad ptr value: " << v << '\n';
          return 2;
        }
        ibuf[i] = p;
        callArgs[i] = &ibuf[i];
      }
    } else {
      int64_t s = 0;
      if (!parseInt(v, s)) {
        std::cerr << "bad int value: " << v << '\n';
        return 2;
      }
      ibuf[i] = (uint64_t)s;
      callArgs[i] = &ibuf[i];
    }
  }
  uint64_t retI = 0;
  double retF = 0;
  void* retPtr = nullptr;
  if (retType == &dinv_type_f32 || retType == &dinv_type_f64)
    retPtr = &retF;
  else if (retType != &dinv_type_void)
    retPtr = &retI;
  dinv_status_t rc = dinv_call(cif, fn, retPtr, callArgs.data());
  if (rc != DINV_OK) {
    std::cerr << "call: " << dinv_last_error() << '\n';
    tdyn_env_destroy(env);
    dinv_cif_destroy(cif);
    return 1;
  }
  if (retType == &dinv_type_f32) {
    float f = 0;
    memcpy(&f, &retF, 4);
    std::cout << f << '\n';
  } else if (retType == &dinv_type_f64) {
    std::cout << retF << '\n';
  } else if (retType != &dinv_type_void) {
    std::cout << (int64_t)retI << '\n';
  }
  tdyn_env_destroy(env);
  dinv_cif_destroy(cif);
  return 0;
}
