"""Compile and execute generated bindings with installed language toolchains.

Run: python -m unittest discover -s tests -v
Missing development toolchains are reported as skips, never stubbed.
"""
from pathlib import Path
import os
import shlex
import shutil
import subprocess
import tempfile
import unittest

from q import QCompiler

ROOT = Path(__file__).resolve().parents[1]
LANGUAGES = ('d', 'go', 'java', 'lua', 'node', 'ocaml', 'ruby', 'rust')


def run(args, **kwargs):
    result = subprocess.run([str(x) for x in args], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kwargs)
    if result.returncode:
        raise AssertionError(f'{args} (exit {result.returncode})\n{result.stdout}')
    return result.stdout


def tool(name):
    path = shutil.which(name)
    if not path:
        raise unittest.SkipTest(f'{name} is not installed')
    return path


def pkg(name, flag):
    try:
        return shlex.split(run([tool('pkg-config'), flag, name]))
    except AssertionError as exc:
        raise unittest.SkipTest(f'{name} development package unavailable') from exc


def flags(lang):
    if lang == 'lua':
        return pkg('lua5.4', '--cflags'), pkg('lua5.4', '--libs')
    if lang == 'ruby':
        version = run([tool('ruby'), '-rrbconfig', '-e', 'print RbConfig::CONFIG["ruby_pc"]']).strip()
        return pkg(version.removesuffix('.pc'), '--cflags'), pkg(version.removesuffix('.pc'), '--libs')
    if lang == 'node':
        candidates = [Path('/usr/include/node'), Path('/usr/local/include/node')]
        if os.environ.get('NODE_INCLUDE_DIR'):
            candidates.insert(0, Path(os.environ['NODE_INCLUDE_DIR']))
        for path in candidates:
            if (path / 'node_api.h').exists():
                return ['-I'+str(path), '-DNAPI_VERSION=6'], []
        raise unittest.SkipTest('Node-API development headers unavailable')
    if lang == 'ocaml':
        return ['-I'+run([tool('ocamlc'), '-where']).strip()], []
    if lang == 'java':
        homes = [Path(os.environ['JAVA_HOME'])] if os.environ.get('JAVA_HOME') else []
        if shutil.which('javac'):
            homes.append(Path(shutil.which('javac')).resolve().parent.parent)
        homes.extend(Path('/usr/lib/jvm').glob('*'))
        for home in homes:
            if (home / 'include/jni.h').exists():
                platform = 'darwin' if os.uname().sysname == 'Darwin' else 'linux'
                lib = home / 'lib/server'
                return ['-I'+str(home/'include'), '-I'+str(home/'include'/platform)], ['-L'+str(lib), '-Wl,-rpath,'+str(lib), '-ljvm']
        raise unittest.SkipTest('JDK/JNI development headers unavailable; set JAVA_HOME')
    return [], []


class Bindings(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='dombind-')
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)
        for lang in LANGUAGES:
            directory = ROOT / f'dom{lang}bind'
            out = self.path / f'dom{lang}bind/capi.h'
            out.parent.mkdir()
            out.write_text(QCompiler(search_path=[str(directory)]).compile(str(directory/f'{lang}bind.q')))

    def build(self, lang, source, *, cpp=False, shared=False, link=True):
        includes, libraries = flags(lang)
        path = self.path / ('test.cpp' if cpp else 'test.c')
        path.write_text(source)
        output = self.path / ('addon.node' if shared else 'test')
        args = [tool('c++' if cpp else 'cc'), '-std=c++17' if cpp else '-std=c99',
                '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-I'+str(self.path),
                '-I'+str(ROOT/f'dom{lang}bind/include'), *includes]
        if shared:
            args += ['-shared', '-fPIC']
        if not link:
            args += ['-fsyntax-only']
        run([*args, path, '-o', output, *libraries])
        return output

    def check_modules(self, lang):
        flags(lang)
        directory = ROOT / f'dom{lang}bind'
        for module in sorted(directory.glob('*.q')):
            compiler = QCompiler(search_path=[str(directory)])
            source = compiler.compile(str(module)) + compiler.compile(str(module), PREFIX='custom_'+module.stem)
            for cpp in (False, True):
                with self.subTest(module=module.name, cpp=cpp):
                    self.build(lang, source, cpp=cpp, link=False)
        self.build(lang, f'#include <dom{lang}bind/dom{lang}bind.hpp>\n', cpp=True, link=False)

    def test_lua_runtime(self):
        program = self.build('lua', r'''
#include <domluabind/domluabind.hpp>
#undef NDEBUG
#include <cassert>
#include <cstring>
int main() {
    domluabind::object retained;
    {
        domluabind::state state;
        lua_State *L = state.get();
        const char bytes[] = {'a', 0, 'b'};
        luaconv_push_string(L, bytes, sizeof bytes);
        retained = domluabind::object(state, -1);
        lua_pop(L, 1);
        lua_gc(L, LUA_GCCOLLECT);
        retained.push();
        size_t n; const char *s = luaconv_string(L, -1, &n);
        assert(n == 3 && memcmp(s, bytes, n) == 0);
        lua_pop(L, 1);
        state.eval("return 6 * 7", 1);
        int ok; assert(luaconv_integer(L, -1, &ok) == 42 && ok);
        lua_pop(L, 1);
        try { state.eval("error('expected')"); assert(false); }
        catch (const domluabind::error &) { assert(lua_gettop(L) == 0); }
        state.eval("return {}", 1);
        domluabind::object table(state, -1), copy(table);
        lua_pop(L, 1);
        copy.push(); luaconv_push_integer(L, 42); luatable_set(L, -2, "answer");
        luatable_get(L, -1, "answer");
        assert(luaconv_integer(L, -1, &ok) == 42 && ok);
    }
    retained.push(); // registry reference owns the state after its wrapper dies
}
''', cpp=True)
        run([program])

    def test_ruby_runtime(self):
        program = self.build('ruby', r'''
#include <domrubybind/domrubybind.hpp>
#undef NDEBUG
#include <cassert>
int main(int argc, char **argv) {
    ruby_sysinit(&argc, &argv);
    RUBY_INIT_STACK;
    assert(rubyinit_setup() == 0);
    rubyinit_loadpath();
    {
        auto v = domrubybind::eval("[40, 2, \"a\\x00b\"]");
        auto copy = v;
        rb_gc_start();
        assert(rubyarray_size(copy.get()) == 3);
        assert(rubyconv_to_int64(rubyarray_get(copy.get(), 0)) == 40);
        assert(rubyconv_string_size(rubyarray_get(copy.get(), 2)) == 3);
        try { domrubybind::eval("raise 'expected'"); assert(false); }
        catch (const domrubybind::error &e) { assert(!NIL_P(e.exception())); }
        auto answer = domrubybind::eval("6 * 7");
        assert(rubyconv_to_int64(answer.get()) == 42);
    }
    return rubyinit_cleanup(0);
}
''', cpp=True)
        run([program])

    def test_node_runtime(self):
        node = tool('node')
        program = self.build('node', r'''
#include <domnodebind/domnodebind.hpp>
#undef NDEBUG
#include <cassert>
static napi_value echo(napi_env env, napi_callback_info info) {
    return domnodebind::callback(env, [&]() {
        napi_value argv[1]; size_t argc = 1;
        domnodebind::check(nodecall_args(env, info, &argc, argv, nullptr, nullptr));
        if (argc != 1) throw std::invalid_argument("one argument required");
        napi_value box; domnodebind::check(nodeobj_new(env, &box));
        domnodebind::check(nodeobj_set(env, box, "value", argv[0]));
        domnodebind::reference ref(env, box), copy(ref);
        napi_value result;
        domnodebind::check(nodeobj_get(env, copy.get(), "value", &result));
        return result;
    });
}
static napi_value init(napi_env env, napi_value exports) {
    return domnodebind::callback(env, [&]() {
        napi_value fn; domnodebind::check(nodecall_new(env, "echo", echo, nullptr, &fn));
        domnodebind::check(nodeobj_set(env, exports, "echo", fn));
        return exports;
    });
}
nodemodule_define(dombind_test, init)
''', cpp=True, shared=True)
        script = self.path/'test.js'
        script.write_text("const assert = require('assert');\nconst m = require('./addon.node');\n"
                          "assert.strictEqual(m.echo('a\\0b'), 'a\\0b');\n"
                          "assert.throws(() => m.echo(), /one argument/);\n"
                          "assert.strictEqual(m.echo(42), 42);\n")
        run([node, script])

    def test_ocaml_runtime(self):
        ocamlc = tool('ocamlc')
        includes, _ = flags('ocaml')
        source = self.path/'stub.c'
        source.write_text(r'''
#include <domocamlbind/capi.h>
#undef NDEBUG
#include <assert.h>
#include <string.h>
CAMLprim value test_bindings(value input) {
    CAMLparam1(input);
    CAMLlocal3(pair, result, binary);
    binary = ocamlconv_string("a\0b", 3);
    pair = ocamlblock_pair(input, binary);
    value root = pair;
    ocamlroot_retain(&root);
    const value *collect = ocamlcall_named("collect");
    assert(collect);
    result = ocamlcall_call(*collect, Val_unit);
    assert(!ocamlcall_is_exception(result));
    assert(ocamlconv_to_int(ocamlblock_get(root, 0)) == 42);
    binary = ocamlblock_get(root, 1);
    assert(ocamlconv_string_size(binary) == 3);
    assert(memcmp(ocamlconv_string_data(binary), "a\0b", 3) == 0);
    const value *fail = ocamlcall_named("fail");
    assert(fail);
    result = ocamlcall_call(*fail, Val_unit);
    assert(ocamlcall_is_exception(result));
    ocamlroot_release(&root);
    CAMLreturn(Val_unit);
}
''')
        ml = self.path/'test.ml'
        ml.write_text('external test : int -> unit = "test_bindings"\n'
                      'let () = Callback.register "collect" (fun () -> Gc.compact ());\n'
                      'Callback.register "fail" (fun () -> failwith "expected"); test 42\n')
        run([tool('cc'), '-Wall', '-Wextra', '-Werror', '-I'+str(self.path), *includes, '-c', source, '-o', self.path/'stub.o'])
        run([ocamlc, '-custom', '-o', self.path/'test', self.path/'stub.o', ml], cwd=self.path)
        run([self.path/'test'])

    def test_java_runtime(self):
        program = self.build('java', r'''
#include <domjavabind/domjavabind.hpp>
#undef NDEBUG
#include <cassert>
int main() {
    JavaVM *vm = nullptr; JNIEnv *env = nullptr;
    JavaVMInitArgs args = {}; args.version = JNI_VERSION_1_8;
    assert(javainit_create(&vm, &env, &args) == JNI_OK);
    {
        domjavabind::local_frame frame(env);
        const jchar text[] = {'a', 0, 'b'};
        jstring string = javastr_new(env, text, 3);
        domjavabind::global_ref ref(env, string), copy(ref);
        assert(javastr_size(env, (jstring)copy.get()) == 3);
        jclass cls = javacall_find_class(env, "java/lang/String");
        jmethodID method = javacall_method(env, cls, "length", "()I");
        assert(javacall_integer(env, string, method, nullptr) == 3);
        assert(!javaerr_pending(env));
        assert(!javacall_find_class(env, "dom/bind/Missing"));
        assert(javaerr_pending(env)); javaerr_clear(env);
    }
    assert(javainit_destroy(vm) == JNI_OK);
}
''', cpp=True)
        run([program])

    def test_c_abi_buffers(self):
        for lang in ('d', 'go', 'rust'):
            with self.subTest(language=lang):
                program = self.build(lang, r'''
#include <domLANGbind/domLANGbind.hpp>
#undef NDEBUG
#include <cassert>
#include <cstring>
static int sum(void *context, const unsigned char *data, size_t n) {
    int *out = static_cast<int *>(context);
    for (size_t i = 0; i < n; ++i) *out += data[i];
    return 0;
}
int main() {
    domLANGbind::buffer a("a\0b", 3), b(a), moved(std::move(a));
    assert(a.size() == 0 && moved.size() == 3);
    assert(b.data() != moved.data() && memcmp(b.data(), "a\0b", 3) == 0);
    LANGbuf_t raw = b.release(); LANGbuf_view view;
    assert(LANGbuf_slice(&raw, 1, 2, &view) == 0 && view.data[0] == 0);
    assert(LANGbuf_slice(&raw, SIZE_MAX, 2, &view) == -1);
    int result = 0; LANGcall_t callback = {sum, &result};
    assert(LANGcall_invoke(callback, raw.data, raw.size) == 0 && result == 195);
    LANGbuf_free(&raw); LANGbuf_free(&raw);
    assert(LANGbuf_copy(&raw, nullptr, 1) == -1 && !raw.data);
    assert(LANGbuf_copy(&raw, nullptr, 0) == 0 && raw.size == 0);
}
'''.replace('LANG', lang), cpp=True)
                run([program])

    def test_rust_native(self):
        tool('cargo')
        run(['cargo', 'test', '--offline', '--manifest-path', ROOT/'domrustbind/Cargo.toml', '--target-dir', self.path/'target'])
        library = self.path/'libdomrust.so'
        run(['cargo', 'build', '--offline', '--manifest-path', ROOT/'domrustbind/Cargo.toml', '--target-dir', self.path/'target'])
        run([tool('rustc'), '--edition=2021', '--crate-type=cdylib', ROOT/'domrustbind/examples/export.rs',
             '--extern', 'domrustbind='+str(self.path/'target/debug/libdomrustbind.rlib'), '-o', library])
        self.export_client('rust', library)

    def test_go_native(self):
        tool('go')
        env = dict(os.environ, GOCACHE=str(self.path/'cache'))
        # Some distributions ship go without vet; leave the choice explicit.
        vet = os.environ.get('DOMBIND_GO_VET')
        run(['go', 'test', *(['-vet='+vet] if vet else []), './...'], cwd=ROOT/'domgobind', env=env)
        library = self.path/'libdomgo.so'
        run(['go', 'build', '-buildmode=c-shared', '-o', library, './examples/export'], cwd=ROOT/'domgobind', env=env)
        self.export_client('go', library)

    def test_d_native(self):
        compiler = tool('ldc2')
        source = ROOT/'domdbind/source/domdbind.d'
        run([compiler, '-unittest', '-main', source, '-of='+str(self.path/'unit')])
        run([self.path/'unit'])
        library = self.path/'libdomd.so'
        run([compiler, '-shared', '-relocation-model=pic', '-I'+str(source.parent), source,
             ROOT/'domdbind/examples/export.d', '-of='+str(library)])
        self.export_client('d', library)

    def export_client(self, lang, library):
        # Load through ctypes so each compiled language's actual ABI is exercised.
        script = self.path/'client.py'
        script.write_text('''import ctypes as c
import sys
lib = c.CDLL(sys.argv[1])
fn = getattr(lib, sys.argv[2])
fn.argtypes = [c.c_void_p, c.c_size_t, c.POINTER(c.c_uint64)]
fn.restype = c.c_int
out = c.c_uint64()
assert fn(c.c_char_p(b"a\\0b"), 3, c.byref(out)) == 0
assert out.value == 195
assert fn(None, 1, c.byref(out)) == -1
assert fn(None, 0, c.byref(out)) == 0 and out.value == 0
assert fn(None, 0, None) == -1
''')
        import sys
        run([sys.executable, script, library, f'dom{lang}_sum'])


for language in LANGUAGES:
    setattr(Bindings, f'test_{language}_modules', lambda self, lang=language: self.check_modules(lang))

if __name__ == '__main__':
    unittest.main()
