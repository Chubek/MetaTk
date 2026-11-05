module QBENative;
// Template-only, like XVMNative: no extra D translation unit is needed.
template QBE() {
    extern(C) {
        struct Call { size_t function_, data; }
        pragma(mangle, "metaml_qbe_compile")
        int metaml_qbe_compile(const(Call)* calls, const(size_t)* offsets,
            size_t blockCount, size_t limit, void** module_, char* error, size_t errorSize) nothrow;
        pragma(mangle, "metaml_qbe_entry") void* metaml_qbe_entry(void* module_, size_t block) nothrow;
        pragma(mangle, "metaml_qbe_size") size_t metaml_qbe_size(void* module_) nothrow;
        pragma(mangle, "metaml_qbe_release") void metaml_qbe_release(void* module_) nothrow;
    }
    class Module {
        void* handle;
        this(void* handle) { this.handle = handle; }
        ~this() { if (handle !is null) metaml_qbe_release(handle); handle = null; }
        int call(size_t block, void* state) {
            alias Entry = extern(C) int function(void*);
            return (cast(Entry)metaml_qbe_entry(handle, block))(state);
        }
    }
}
