# qscript --cffi run qscript/examples/cffi.qs
use cffi, endian;
let data=alloc(16);
write(data,0,8,0x0102030405060708);
assert(byte(data,0)==8);
assert(endian::swap64(read(data,0,8))==0x0807060504030201);
copy(data,8,data,0,8);
let result=read(data,8,8);
release(data);
return result;
