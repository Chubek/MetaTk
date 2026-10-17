# Typed C interop. Enable with qs_cffi_enable() or qscript --cffi.
# Float/double and pointers cross the integer VM as bit patterns.
const VOID=0;
const I8=1; const U8=2; const I16=3; const U16=4;
const I32=5; const U32=6; const I64=7; const U64=8;
const FLOAT=9; const DOUBLE=10; const POINTER=11;
fn alloc(bytes) { return _cffi_alloc(bytes); }
fn release(handle) { return _cffi_release(handle); }
fn size(buffer) { return _cffi_size(buffer); }
fn address(buffer) { return _cffi_address(buffer); }
# Little-endian loads/stores; width must be 1, 2, 4 or 8.
fn read(buffer, offset, width) { return _cffi_read(buffer, offset, width); }
fn write(buffer, offset, width, value) { return _cffi_write(buffer, offset, width, value); }
fn byte(buffer, offset) { return read(buffer, offset, 1); }
fn put_byte(buffer, offset, value) { assert(value>=0 && value<=255); return write(buffer, offset, 1, value); }
# path/name are handles to NUL-terminated buffers. open(0) opens this process.
fn open(path) { return _cffi_open(path); }
fn symbol(library, name) { return _cffi_symbol(library, name); }
# Argument types packed in nibbles, first argument in low nibble.
fn function(library, name, result_type, argument_types, count) {
    return _cffi_function(library, name, result_type, argument_types, count);
}
fn arg(types, index, type) {
    assert(index>=0 && index<16 && type>VOID && type<=POINTER);
    return (types & ~(15 << (index*4))) | (type << (index*4));
}
fn call0(f) { return _cffi_call0(f); }
fn call1(f,a) { return _cffi_call1(f,a); }
fn call2(f,a,b) { return _cffi_call2(f,a,b); }
fn call3(f,a,b,c) { return _cffi_call3(f,a,b,c); }
fn call4(f,a,b,c,d) { return _cffi_call4(f,a,b,c,d); }
fn call5(f,a,b,c,d,e) { return _cffi_call5(f,a,b,c,d,e); }
fn call6(f,a,b,c,d,e,g) { return _cffi_call6(f,a,b,c,d,e,g); }
fn call7(f,a,b,c,d,e,g,h) { return _cffi_call7(f,a,b,c,d,e,g,h); }
fn call8(f,a,b,c,d,e,g,h,i) { return _cffi_call8(f,a,b,c,d,e,g,h,i); }
# Byte-wise copies support overlap, like memmove, with checked buffer bounds.
fn copy(dst, dst_offset, src, src_offset, count) {
    assert(count>=0 && dst_offset>=0 && src_offset>=0);
    assert(dst_offset<=size(dst) && count<=size(dst)-dst_offset);
    assert(src_offset<=size(src) && count<=size(src)-src_offset);
    if (dst==src && dst_offset>src_offset) {
        for (let i=count; i>0; i=i-1) put_byte(dst,dst_offset+i-1,byte(src,src_offset+i-1));
    } else {
        for (let i=0; i<count; i=i+1) put_byte(dst,dst_offset+i,byte(src,src_offset+i));
    }
    return count;
}
