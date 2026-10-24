/++ MetaArch ABI v1 D interface. Link with libmetaarch; this module needs no
    generated C headers or dependency on the internal Dsym model layout.

    Handles are owned: use the matching free function (including with scope(exit)).
    View strings/bytes are borrowed until owner destruction; diagnostic views
    also expire on clear. Copy them if a longer lifetime is required. Views have
    native C alignment, never pack them. Read-only spec operations may run in
    parallel; free/backend mutation/diagnostics mutation require exclusion.
    NULL create/load/encode/write means failure; diagnostics are optional.
    This module intentionally exposes the C ABI with explicit ownership rather
    than a GC finalizer that could race with calls or foreign library shutdown.
 +/
module metaarch;
import object : size_t;

struct MaSpec;
struct MaDiagnostics;
struct MaResult;
alias MaStatus = int;
alias MaFormat = int;
enum uint METAARCH_ABI_VERSION = 1;
enum : int { MA_OK = 0, MA_INVALID_ARGUMENT = 1, MA_NOT_FOUND = 2 }
enum : int { MA_AUTO = 0, MA_YAML = 1, MA_JSON = 2, MA_XML = 3, MA_SEXPR = 4 }
enum : int { MA_NOTE = 0, MA_WARNING = 1, MA_ERROR = 2 }
enum : uint { MA_LITTLE_ENDIAN = 0, MA_BIG_ENDIAN = 1 }
enum : uint { MA_LOAD_BACKEND = 1, MA_LOAD_NO_COMPILE = 2, MA_LOAD_STRICT = 4, MA_LOAD_LINT = 8 }
enum : uint { MA_WRITE_COMPACT = 1, MA_WRITE_EMBED_BACKEND = 2,
              MA_WRITE_REF_BACKEND = 4, MA_WRITE_NO_HEADER = 8, MA_WRITE_SORT_KEYS = 16 }

extern(C) struct MaDiagnosticView {
    int severity;
    uint line, column;
    const(char)* file, path, message;
}
extern(C) struct MaTargetView {
    const(char)* arch, subarch, object_format, abi;
    long address_width;
    uint byte_order;
}
extern(C) struct MaRegisterView {
    const(char)* name, reg_class;
    long width, encoding;
}
extern(C) struct MaFormView {
    const(char)* mnemonic, encoder;
    size_t operand_count, visible_operand_count;
}
extern(C) struct MaRelocationView {
    const(char)* name;
    uint offset, size, bit_offset, bit_width, byte_order, symbol_parameter;
    long symbol_value;
}

extern(C) nothrow @nogc {
    uint metaarch_abi_version();
    MaDiagnostics* metaarch_diagnostics_create();
    void metaarch_diagnostics_free(MaDiagnostics* d);
    void metaarch_diagnostics_clear(MaDiagnostics* d);
    size_t metaarch_diagnostics_count(const(MaDiagnostics)* d);
    MaStatus metaarch_diagnostics_get(const(MaDiagnostics)* d, size_t index, MaDiagnosticView* out_);
    MaSpec* metaarch_spec_load(const(char)* path, MaFormat format, uint flags, MaDiagnostics* d);
    MaSpec* metaarch_spec_parse(const(void)* data, size_t len, MaFormat format,
                               const(char)* virtual_path, uint flags, MaDiagnostics* d);
    void metaarch_spec_free(MaSpec* s);
    int metaarch_spec_load_backend(MaSpec* s, MaDiagnostics* d);
    size_t metaarch_spec_target_count(const(MaSpec)* s);
    size_t metaarch_spec_register_count(const(MaSpec)* s);
    size_t metaarch_spec_encoder_count(const(MaSpec)* s);
    size_t metaarch_spec_form_count(const(MaSpec)* s);
    MaStatus metaarch_spec_target(const(MaSpec)* s, size_t index, MaTargetView* out_);
    MaStatus metaarch_spec_register(const(MaSpec)* s, size_t index, MaRegisterView* out_);
    MaStatus metaarch_spec_form(const(MaSpec)* s, size_t index, MaFormView* out_);
    const(char)* metaarch_spec_encoder_name(const(MaSpec)* s, size_t index);
    int metaarch_spec_find_register(const(MaSpec)* s, const(char)* name);
    int metaarch_spec_find_encoder(const(MaSpec)* s, const(char)* name);
    MaResult* metaarch_spec_write(const(MaSpec)* s, MaFormat format, uint flags, MaDiagnostics* d);
    MaResult* metaarch_spec_encode(const(MaSpec)* s, const(char)* encoder, size_t target,
                                  const(char*)* names, const(long)* values, size_t n, MaDiagnostics* d);
    MaResult* metaarch_spec_encode_form(const(MaSpec)* s, size_t form, size_t target,
                                       const(long)* values, size_t n, MaDiagnostics* d);
    void metaarch_result_free(MaResult* r);
    const(ubyte)* metaarch_result_data(const(MaResult)* r);
    size_t metaarch_result_size(const(MaResult)* r);
    size_t metaarch_result_relocation_count(const(MaResult)* r);
    MaStatus metaarch_result_relocation(const(MaResult)* r, size_t index, MaRelocationView* out_);
}
