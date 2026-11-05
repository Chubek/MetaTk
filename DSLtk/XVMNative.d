module XVMNative;
version(AArch64) extern(C) void __clear_cache(void*, void*);
version(RISCV64) extern(C) void __clear_cache(void*, void*);
// Template-only: importing MetaML still needs only MetaML.d and DSLtk.d linked.
template Native() {
    enum Target { Amd64, Riscv64, Aarch64, Unsupported }
    Target host() {
        version(BigEndian) return Target.Unsupported;
        else version(X86_64) return Target.Amd64;
        else version(AArch64) return Target.Aarch64;
        else version(RISCV64) return Target.Riscv64;
        else return Target.Unsupported;
    }
    void little(ref char[] output, ulong value, uint bytes) {
        foreach (i; 0 .. bytes) output ~= cast(char)(value >> (8 * i));
    }
    void patch(ref char[] output, size_t at, ulong value, uint bytes) {
        foreach (i; 0 .. bytes) output[at + i] = cast(char)(value >> (8 * i));
    }
    struct Call { size_t function_, data; }
    string block(Target target, Call[] calls) {
        if (!calls.length || calls.length > 32) throw new Exception("invalid native block size");
        char[] output;
        size_t[] exits;
        void word(uint n) { little(output, n, 4); }
        void armAddress(uint reg, size_t value) {
            foreach (uint i; 0 .. 4) word(cast(uint)((i ? 0xf2800000U : 0xd2800000U) | (i << 21) | (((value >> (16 * i)) & 65535) << 5) | reg));
        }
        void rvAddress(uint reg, size_t value) {
            if (output.length % 8) word(0x00000013);
            word((reg << 7) | 0x17); word(0x01003003 | (reg << 15) | (reg << 7));
            word(0x0100006f); word(0x00000013); little(output, value, 8);
        }
        final switch(target) {
        case Target.Amd64:
            output ~= "\xf3\x0f\x1e\xfa\x53";
            version(Windows) output ~= "\x48\x83\xec\x20\x48\x89\xcb";
            else output ~= "\x48\x89\xfb";
            foreach (call; calls) {
                version(Windows) output ~= "\x48\x89\xd9\x48\xba";
                else output ~= "\x48\x89\xdf\x48\xbe";
                little(output, call.data, 8); output ~= "\x48\xb8"; little(output, call.function_, 8);
                output ~= "\xff\xd0\x85\xc0\x0f\x85"; exits ~= output.length; little(output, 0, 4);
            }
            auto end = output.length;
            foreach (at; exits) patch(output, at, end - at - 4, 4);
            version(Windows) output ~= "\x48\x83\xc4\x20";
            output ~= "\x5b\xc3"; break;
        case Target.Aarch64:
            word(0xd503245f); word(0xa9bf7bf3); word(0xaa0003f3);
            foreach (call; calls) {
                word(0xaa1303e0); armAddress(1, call.data); armAddress(16, call.function_); word(0xd63f0200);
                exits ~= output.length; word(0);
            }
            foreach (at; exits) patch(output, at, 0x35000000U | (((output.length - at) / 4) << 5), 4);
            word(0xa8c17bf3); word(0xd65f03c0); break;
        case Target.Riscv64:
            word(0xff010113); word(0x00113423); word(0x00813023); word(0x00050413);
            foreach (call; calls) {
                word(0x00040513); rvAddress(11, call.data); rvAddress(5, call.function_); word(0x000280e7);
                exits ~= output.length; word(0);
            }
            foreach (at; exits) {
                auto n = cast(uint)(output.length - at);
                patch(output, at, 0x00051063U | ((n >> 12) << 31) | (((n >> 5) & 63) << 25) | (((n >> 1) & 15) << 8) | (((n >> 11) & 1) << 7), 4);
            }
            word(0x00013403); word(0x00813083); word(0x01010113); word(0x00008067); break;
        case Target.Unsupported: throw new Exception("unsupported native architecture");
        }
        return cast(string)output;
    }
    class Executable {
        void* memory;
        size_t size;
        this(string code) {
            import core.stdc.string : memcpy;
            size = code.length;
            if (!size) throw new Exception("empty native code");
            if (size > size_t.max - 16) throw new Exception("native code size overflow");
            size += 16;
            version(METAML_NO_JIT) throw new Exception("JIT executable memory is unavailable");
            else version(Windows) {
                import core.sys.windows.windows;
                memory = VirtualAlloc(null, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
                if (memory is null) throw new Exception("JIT allocation failed");
                import core.stdc.string : memset;
                memset(memory, 0, 16); memcpy(cast(ubyte*)memory + 16, code.ptr, code.length);
                DWORD old;
                if (!VirtualProtect(memory, size, PAGE_EXECUTE_READ, &old) || !FlushInstructionCache(GetCurrentProcess(), memory, size)) {
                    VirtualFree(memory, 0, MEM_RELEASE); memory = null;
                    throw new Exception("JIT executable memory is unavailable");
                }
            } else version(Posix) {
                import core.sys.posix.sys.mman;
                memory = mmap(null, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
                if (memory == MAP_FAILED) { memory = null; throw new Exception("JIT allocation failed"); }
                import core.stdc.string : memset;
                memset(memory, 0, 16); memcpy(cast(ubyte*)memory + 16, code.ptr, code.length);
                if (mprotect(memory, size, PROT_READ | PROT_EXEC)) {
                    munmap(memory, size); memory = null;
                    throw new Exception("JIT executable memory is unavailable");
                }
                version(AArch64) __clear_cache(memory, cast(ubyte*)memory + size);
                version(RISCV64) __clear_cache(memory, cast(ubyte*)memory + size);
            } else throw new Exception("JIT executable memory is unavailable");
        }
        ~this() {
            if (memory is null) return;
            version(Windows) { import core.sys.windows.windows; VirtualFree(memory, 0, MEM_RELEASE); }
            else version(Posix) { import core.sys.posix.sys.mman; munmap(memory, size); }
            memory = null;
        }
        int call(void* data) {
            alias Entry = extern(C) int function(void*);
            return (cast(Entry)(cast(ubyte*)memory + 16))(data);
        }
    }
}
