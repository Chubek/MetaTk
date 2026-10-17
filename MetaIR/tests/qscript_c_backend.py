"""Compile generated C using the configured C compiler and compare VM results."""
import pathlib
import subprocess
import sys
import tempfile

cli, cc = sys.argv[1:3]
cffi = sys.argv[3].upper() in ("ON", "1", "TRUE", "YES")
runtime = sys.argv[4]
sanitizers = sys.argv[5].upper() in ("ON", "1", "TRUE", "YES")
repository = pathlib.Path(__file__).resolve().parent.parent

def run(command, **kwargs):
    return subprocess.run(command, capture_output=True, timeout=60, **kwargs)


def compile_c(source, generated, exe, standalone=True):
    flags = ["-DQSCRIPT_STANDALONE"] if standalone else []
    if "QScript linked runtime:" in generated.read_text():
        flags += ["-I", str(repository), runtime, "-Wl,-rpath," + str(pathlib.Path(runtime).resolve().parent)]
        if sanitizers:
            flags += ["-fsanitize=address,undefined"]
    return run([cc, "-std=c99", "-Wall", "-Wextra", "-pedantic", "-O2", str(source), *flags, "-o", str(exe)])

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
        "grammar G { token TOP { 'a' } } print G.parse(\"a\").text(); return 7;",
        "let g=compile_grammar(\"grammar G { token TOP { 'a' } }\"); print g.parse(\"b\").ok(); return 0;",
        "let gs:list<grammar>=[]; print gs; return 0;",
        "grammar G { token TOP { 'a' } } print G.parse(\"a\",\"missing\").text();",
        "let g=compile_grammar(\"not a grammar\"); print g.name();",
    ]
    for index, source in enumerate(cases):
        qs = root / "input.qs"
        c = root / "generated.c"
        exe = root / "generated"
        qs.write_text(source)
        flags = ["--steps", "100"] if index in (10, 11) else ["--steps", "100000"]
        emitted = run([cli, *flags, "c", str(qs), "-o", str(c)])
        assert emitted.returncode == 0, emitted.stderr
        compiled = compile_c(c, c, exe)
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
    compiled = compile_c(harness, c, exe, standalone=False)
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
        compiled = compile_c(harness, c, exe, standalone=False)
        assert compiled.returncode == 0, compiled.stderr
        assert run([str(exe)]).returncode == 0

    # A linked grammar runtime is recreated for each run and forwards callback
    # failures. This is compiled as C99, not as C++ or an embedded VM fallback.
    qs.write_text('grammar G { token TOP { "a" } } print G.parse("a").text(); return 7;')
    assert run([cli, "c", str(qs), "-o", str(c)]).returncode == 0
    harness.write_text('''#include "generated.c"
static int text(const char *p,size_t n,void *user) {
    if(n!=1 || p[0]!='a') return 1;
    ++*(int*)user; return 0;
}
static int fail_text(const char *p,size_t n,void *user) { (void)p; (void)n; (void)user; return 1; }
int main(void) {
    int64_t result=0; int count=0;
    qs_generated_set_text(text,&count);
    if(qs_generated_run(10000,128,&result,NULL,NULL,NULL) || result!=7 || count!=1) return 1;
    if(qs_generated_run(10000,128,&result,NULL,NULL,NULL) || result!=7 || count!=2) return 2;
    if(qs_generated_run(0,128,&result,NULL,NULL,NULL)!=2) return 3;
    qs_generated_set_text(fail_text,NULL);
    if(qs_generated_run(10000,128,&result,NULL,NULL,NULL)!=4) return 4;
    qs_generated_set_text(NULL,NULL);
    if(qs_generated_run(10000,128,&result,NULL,NULL,NULL)!=1) return 5;
    return 0;
}
''')
    compiled = compile_c(harness, c, exe, standalone=False)
    assert compiled.returncode == 0 and not compiled.stderr, compiled.stderr
    result = run([str(exe)])
    assert result.returncode == 0, result
print("QScript portable C backend passed")
