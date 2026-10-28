local api=require('metail.api')
local ir=require('metail.ir')
local source=[[
module foreign;
extern fn magnitude(i64) -> i64 from "c" symbol "labs";
extern fn putchar(i32) -> i32 from "c";
extern fn custom(i64, ptr) -> i64 from "./a path/libcustom.so" symbol "custom_fn";
extern fn print(ptr, ...) -> i32 from "c" symbol "printf";
extern fn release(ptr) -> void from "c" symbol "free";
fn main(x) {
  eval release(0);
  eval putchar(65);
  return custom(magnitude(x), 0) * 0;
}
]]
local p=api.parse(source,'foreign.mi')
local text=ir.emit(p)
assert(ir.emit(api.parse(text))==text)
assert(#p.externals==5 and p.externals[1].symbol=='labs')
local meta=ir.linkage(p)
assert(#meta.libraries==2 and meta.arguments[1]=='-lc' and meta.arguments[2]=='./a path/libcustom.so')
assert(meta.imports[4].variadic and meta.imports[5].result=='void')
assert(meta.imports[3].parameters[2]=='ptr' and meta.imports[1].calling_convention=='c')
local optimized=api.optimize_ir(p,'all',2)
assert(ir.emit(optimized):find('custom%(magnitude%(x%), 0%)')) -- not discarded by * 0
assert(#optimized.externals==5 and optimized.externals[3].library=='./a path/libcustom.so')
assert(api.linkage(text)==api.module_linkage(optimized))
assert(api.analyze(text,nil,'ControlFlowGraph'):find('ControlFlowGraph'))
assert(api.check('fn legacy() { return unknown(1); }')==nil) -- unresolved legacy calls remain legal
for _,bad in ipairs({
  'extern fn f(i64) -> i64 from "c"; fn main(){ return f(); }',
  'extern fn f(i64) -> i64 from "c"; fn main(){ return f(1,2); }',
  'extern fn f(ptr, ...) -> i32 from "c"; fn main(){ return f(); }',
  'extern fn f(...) -> i32 from "c"; fn main(){ return 0; }',
  'extern fn f(void) -> i64 from "c"; fn main(){ return 0; }',
  'extern fn f(f64) -> i64 from "c"; fn main(){ return 0; }',
  'extern fn f() -> missing from "c"; fn main(){ return 0; }',
  'extern fn f() -> void from "c"; fn main(){ return f(); }',
  'extern fn f() -> void from "c"; fn main(){ eval add(f(),1); return 0; }',
  'extern fn f() -> i64 from "c"; fn f(){ return 0; }',
  'extern fn f() -> i64 from "c"; extern fn f() -> i64 from "m"; fn main(){return 0;}',
  'extern fn a() -> i64 from "c" symbol "f"; extern fn b() -> i64 from "m" symbol "f"; fn main(){return 0;}',
  'extern fn f() -> i64 from ""; fn main(){ return 0; }',
  'extern fn f() -> i64 from "-Wl,evil"; fn main(){ return 0; }',
  'extern fn f() -> i64 from "@response/file"; fn main(){ return 0; }',
  'extern fn f() -> i64 from "a;b"; fn main(){ return 0; }',
  'extern fn f() -> i64 from "c\\nret"; fn main(){ return 0; }',
  'extern fn f() -> i64 from "c" symbol "f;evil"; fn main(){ return 0; }',
  'extern fn f() -> i64 from "unterminated',
  'extern fn f() -> i64 from "bad\\q"; fn main(){return 0;}',
  'fn main(){return "-" 1;}',
  'fn main(){return "(" 1);}',
  'fn main(){return 1 "+" 2;}',
  'fn main(){"return" 1;}',
  'fn main(){"let" x=1; return x;}',
}) do
  local ok=pcall(api.parse,bad,'bad.mi');assert(not ok,'accepted invalid external: '..bad)
end
api.check('extern fn f(ptr, ...) -> i32 from "c"; fn main(){return f(0,1,2);}')
print('external signatures, metadata, effects, and parser checks passed')
