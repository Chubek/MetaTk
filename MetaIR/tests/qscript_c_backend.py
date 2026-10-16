"""Compile generated C using the configured C compiler and compare VM results."""
import pathlib
import subprocess
import sys
import tempfile

cli, cc = sys.argv[1:3]
cffi = sys.argv[3].upper() in ("ON", "1", "TRUE", "YES")

def run(command, **kwargs):
    return subprocess.run(command, capture_output=True, timeout=60, **kwargs)

with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    cases = [
        "return 42;",
        "print -9223372036854775808; return 9223372036854775807+1;",
        "fn f(n) { if(n<2) return n; return f(n-1)+f(n-2); } print f(10); return f(12);",
        "let x=0; for(let i=0;i<20;i++) { if(i%2) continue; x+=i; } return x;",
        "use endian, integer; print swap64(-7); return midpoint(MIN,MAX);",
        "print -7/3, -7%3, -1>>63, 1<<63; return 0;",
        "return 7/0;",
        "return -9223372036854775808/-1;",
        "return 1<<64;",
        "assert(0);",
        "while(1) {}",
        "fn f(n) { return f(n+1); } return f(0);",
    ]
    for index, source in enumerate(cases):
        qs = root / "input.qs"
        c = root / "generated.c"
        exe = root / "generated"
        qs.write_text(source)
        flags = ["--steps", "100000"] if index < 10 else ["--steps", "100"]
        emitted = run([cli, *flags, "c", str(qs), "-o", str(c)])
        assert emitted.returncode == 0, emitted.stderr
        compiled = run([cc, "-std=c99", "-O2", "-DQSCRIPT_STANDALONE", str(c), "-o", str(exe)])
        assert compiled.returncode == 0, compiled.stderr
        native = run([str(exe)])
        vm = run([cli, *flags, "run", str(qs)])
        if vm.returncode == 0:
            assert native.returncode == 0 and native.stdout == vm.stdout, (source, native, vm)
        else:
            assert native.returncode in (121, 122, 123), (source, native)

    # Exercise the generated embedding ABI and stack ceilings independently.
    qs.write_text("fn f(n) { return n+1; } print f(41); return f(6);")
    assert run([cli, "c", str(qs), "-o", str(c)]).returncode == 0
    harness = root / "harness.c"
    harness.write_text('''#include "generated.c"
static int collect(int64_t n,void *user) { *(int64_t*)user+=n; return 0; }
int main(void) {
    int64_t result=0,total=0;
    if(qs_generated_run(10000,128,&result,NULL,collect,&total) || result!=7 || total!=42) return 1;
    if(qs_generated_run(0,128,&result,NULL,NULL,NULL)!=2) return 2;
    if(qs_generated_run(10000,0,&result,NULL,NULL,NULL)!=3) return 3;
    if(qs_generated_run(10000,SIZE_MAX,&result,NULL,NULL,NULL)!=3) return 4;
    return 0;
}
''')
    compiled = run([cc, "-std=c99", "-O2", str(harness), "-o", str(exe)])
    assert compiled.returncode == 0, compiled.stderr
    assert run([str(exe)]).returncode == 0
    if cffi:
        qs.write_text("return _cffi_size(42);")
        emitted = run([cli, "--cffi", "c", str(qs), "-o", str(c)])
        assert emitted.returncode == 0, emitted.stderr
        harness.write_text('''#include "generated.c"
static int host(int64_t id,const int64_t *args,size_t count,int64_t *result,void *user) {
    (void)user;
    if(id<=0 || count!=32 || args[0]!=42) return 1;
    *result=args[0]+1; return 0;
}
int main(void) {
    int64_t result=0;
    if(qs_generated_run(10000,128,&result,host,NULL,NULL) || result!=43) return 1;
    if(qs_generated_run(10000,128,&result,NULL,NULL,NULL)!=4) return 2;
    return 0;
}
''')
        compiled = run([cc, "-std=c99", "-O2", str(harness), "-o", str(exe)])
        assert compiled.returncode == 0, compiled.stderr
        assert run([str(exe)]).returncode == 0
print("QScript portable C backend passed")
