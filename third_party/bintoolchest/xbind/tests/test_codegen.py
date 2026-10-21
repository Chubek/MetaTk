#!/usr/bin/env python3
"""Executable XBind contracts; compiler/runtime failures are never skips."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest

from regimen import REPOSITORY_SPECS


def run(args, **kwargs):
    return subprocess.run([str(a) for a in args], text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, timeout=60, **kwargs)


class Codegen(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="xbind-", dir=ARGS.work)
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)

    def cli(self, command, spec):
        return run([ARGS.cli, command, spec])

    def good(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout

    def spec(self, body):
        path = self.path / "test.xbind"
        path.write_text('local x = require("lxbind")\n' + body, encoding="utf-8")
        return path

    def test_every_repository_spec_has_deterministic_ir_and_exact_signatures(self):
        actual = {str(p.relative_to(ARGS.root.parents[1])) for p in ARGS.root.parents[1].rglob("*.xbind")
                  if ".git" not in p.parts and "third_party" not in p.parts}
        self.assertEqual(actual, set(REPOSITORY_SPECS))
        for relative, name in REPOSITORY_SPECS.items():
            with self.subTest(spec=relative):
                spec = ARGS.root.parents[1] / relative
                text = self.good(self.cli("--dump-ir", spec))
                self.assertEqual(text, self.good(self.cli("--dump-ir", spec)))
                ir = json.loads(text)
                self.assertEqual(ir["library"], name)
                self.assertEqual(len(ir["targets"]), 9)
                for header in ir["headers"]:
                    self.assertTrue((spec.parent / header).is_file(), header)
                if not ir["functions"]:
                    self.assertIn("inventory-only", ir["features"])
                    rejected = self.cli("--emit-lua", spec)
                    self.assertNotEqual(rejected.returncode, 0)
                    self.assertIn("inventory-only", rejected.stderr)
                    continue
                source = self.path / "check.cpp"
                source.write_text(self.good(self.cli("--emit-check", spec)), encoding="utf-8")
                self.good(run([ARGS.cxx, "-std=c++20", "-fsyntax-only", "-I" + str(spec.parent),
                               "-I" + str(ARGS.root.parents[1] / "limestone"), source]))

    def test_rejects_malformed_manifests_in_both_runtimes(self):
        cases = [
            ('return x.library{name=""}', "non-empty"),
            ('return x.library{name="a",targets={x.target("NoSuch")}}', "unknown target"),
            ('return x.library{name="a",targets={x.target("Lua"),x.target("Lua")}}', "duplicate target"),
            ('return x.library{name="a",functions={x.func("f"),x.func("f")}}', "duplicate function"),
            ('return x.library{name="a",functions={x.func("f",{params={x.param("p","int"),x.param("p","int")}})}}', "duplicate parameter"),
            ('return x.library{name="a",functions={x.func("f",{params={x.param("p","void")}})}}', "void parameter"),
            ('return x.library{name="a",functions={x.func("f",{params={x.param("p","int",{nullable=true})}})}}', "nullable requires"),
            ('return x.library{name="a",functions={x.func("f",{params={x.param("p","void *",{nullable="yes"})}})}}', "nullable must"),
            ('return x.library{name="a",headers={[2]="missing.h"}}', "array"),
            ('return x.library{name="a",functions={not_an_index=x.func("f")}}', "array"),
            ('return x.library{name="a",typedefs={x.typedef("T","int"),x.typedef("T","string")}}', "duplicate typedef"),
            ('return x.library{name="a",functions={{name="f",returns="int"}}}', "function"),
            ('return x.library{name="a",modules={x.module("m")}}', "module lowering"),
            ('return x.library{name="a",version=1}', "string"),
            ('return x.library{name="a",functions={x.func("bad-name")}}', "identifier"),
            ('return x.library{name="a",functions={x.func("f",{returns="int; injected"})}}', "C type"),
            ('return x.library{name="a",targets={x.target("lua")}}', "unknown target"),
        ]
        lua = shutil.which("lua5.4") or shutil.which("lua")
        for body, diagnostic in cases:
            with self.subTest(body=body):
                path = self.spec(body)
                result = self.cli("--check-spec", path)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(diagnostic, result.stderr)
                if lua:
                    script = "package.path=" + json.dumps(str(ARGS.root / "lxbind/?.lua") + ";") + "..package.path; "
                    script += "local m=assert(loadfile(" + json.dumps(str(path)) + "))(); require('lxbind').validate_library(m)"
                    result = run([lua, "-e", script])
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(diagnostic, result.stderr)

    def test_options_are_preserved_and_constructor_options_are_not_mutated(self):
        path = self.spec('''local opts={nullable=true,ownership="borrowed"}
local a=x.param("a","const char *",opts)
local b=x.param("b","const char *",opts)
return x.library{name="a",features={"contract"},targets={x.target("Lua",{module="custom",typedefs={x.typedef("T","integer")}})},
typedefs={x.typedef("T","int",{range="status"})},functions={x.func("f",{returns="T",params={a,b}})}}''')
        ir = json.loads(self.good(self.cli("--dump-ir", path)))
        self.assertEqual([p["name"] for p in ir["functions"][0]["params"]], ["a", "b"])
        self.assertEqual(ir["functions"][0]["params"][0]["options"], {"nullable": True, "ownership": "borrowed"})
        self.assertEqual(ir["targets"][0]["options"]["module"], "custom")
        self.assertEqual(ir["typedefs"][0]["options"]["range"], "status")

    def test_signature_mismatch_is_a_compiler_error(self):
        (self.path / "wrong.h").write_text("double wrong(int);\n", encoding="utf-8")
        spec = self.spec('return x.library{name="wrong",headers={"wrong.h"},functions={x.func("wrong",{returns="int",params={x.param("v","int")}})}}')
        check = self.path / "wrong.cpp"
        check.write_text(self.good(self.cli("--emit-check", spec)), encoding="utf-8")
        result = run([ARGS.cxx, "-std=c++20", "-fsyntax-only", "-I" + str(self.path), check])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("XBind signature mismatch", result.stderr)

    def test_spec_resolution_priority_and_directories_are_not_specs(self):
        directory = self.path / "directory.xbind"
        directory.mkdir()
        result = self.cli("--find-spec", directory)
        self.assertNotEqual(result.returncode, 0)
        for suffix in [".xbind", ".lua"]:
            (self.path / ("priority" + suffix)).write_text('return {}', encoding="utf-8")
        found = self.good(run([ARGS.cli, "--find-spec", "priority", self.path]))
        self.assertTrue(found.strip().endswith("priority.xbind"))
        (self.path / "priority.xbind").unlink()
        found = self.good(run([ARGS.cli, "--find-spec", "priority", self.path]))
        self.assertTrue(found.strip().endswith("priority.lua"))

    def test_unsupported_lowering_fails_without_partial_output(self):
        cases = [
            ('typedefs={},functions={x.func("f",{returns="unknown_t"})}', "unmapped Lua type"),
            ('typedefs={x.typedef("int","table")},functions={x.func("f",{returns="int"})}', "unsupported Lua native type"),
            ('typedefs={x.typedef("char *","string",{ownership="owned"})},functions={x.func("f",{returns="char *"})}', "borrowed const char"),
        ]
        for fields, diagnostic in cases:
            spec = self.spec('return x.library{name="a",targets={x.target("Lua")},' + fields + '}')
            result = self.cli("--emit-lua", spec)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(result.stdout, "")
            self.assertIn(diagnostic, result.stderr)
        result = self.cli("--emit-lua", ARGS.root.parents[1] / "yellowstone/Yellowstone.xbind")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("out-buffer", result.stderr)
        self.assertEqual(result.stdout, "")

    def test_target_override_precedence_and_module_option(self):
        spec = self.spec('''return x.library{name="a",targets={x.target("Lua",{module="custom",typedefs={x.typedef("T","integer")}})},
typedefs={x.typedef("T","unsupported")},functions={x.func("f",{returns="T"})}}''')
        generated = self.good(self.cli("--emit-lua", spec))
        self.assertIn('luaopen_custom', generated)
        self.assertIn('xb_push_integer(L, result)', generated)

    def test_typedef_map_parser_rejects_corruption(self):
        directory = self.path / "typedefs"
        directory.mkdir()
        env = dict(os.environ, XBIND_DATA_DIR=str(self.path))
        for bad in ['(xbind-typedefs', '(xbind-typedefs (target "Rust"))',
                    '(xbind-typedefs (target "Lua") (map (c "int")))',
                    '(xbind-typedefs (target "Lua")) trailing']:
            with self.subTest(map=bad):
                (directory / "Lua-Default.sexpr").write_text(bad, encoding="utf-8")
                result = run([ARGS.cli, "--emit-lua", ARGS.root / "examples/hello/hello.xbind"], env=env)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, "")
                self.assertIn("typedef", result.stderr)

    def test_lua_generated_hello_and_self_binding_execute(self):
        if os.name == "nt":
            self.skipTest("generated-module harness currently uses POSIX compiler flags")
        lua = shutil.which("lua5.4") or shutil.which("lua")
        pkg = shutil.which("pkg-config")
        if not lua or not pkg:
            self.skipTest("Lua interpreter or pkg-config absent")
        flags = run([pkg, "--cflags", "--libs", "lua5.4"])
        if flags.returncode:
            self.skipTest("Lua 5.4 development SDK absent")
        # Generate the real DomLuaBind C API from Q; no handwritten substitute.
        env = dict(os.environ)
        env["PYTHONPATH"] = str(ARGS.root / "dombind/domqlib") + os.pathsep + env.get("PYTHONPATH", "")
        self.good(run([sys.executable, "-B", ARGS.root / "dombind/domluabind/generate.py",
                       self.path / "domluabind/capi.h"], env=env))
        for spec, native, module in [
            (ARGS.root / "examples/hello/hello.xbind", ARGS.root / "examples/hello/hello.c", "hello"),
            (ARGS.root / "Dogfooding.xbind", ARGS.root / "xbind.c", "dogfooding"),
        ]:
            glue = self.path / (module + ".cpp")
            glue.write_text(self.good(self.cli("--emit-lua", spec)), encoding="utf-8")
            obj = self.path / (module + ".o")
            self.good(run([ARGS.cc, "-std=c11", "-fPIC", "-c", native, "-o", obj]))
            self.good(run([ARGS.cxx, "-std=c++20", "-shared", "-fPIC", "-Wall", "-Wextra", "-Werror",
                           "-I" + str(self.path), "-I" + str(spec.parent), glue, obj,
                           *shlex.split(flags.stdout), "-o", self.path / (module + ".so")]))
        script = '''local h=require('hello')
assert(h.hello_add(2,5)==7)
assert(h.hello_scale(2.5,4)==10)
assert(h.hello_greet(nil)~=nil)
assert(h.hello_greet('world'):find('world'))
for _,args in ipairs({{1},{1,2,3},{'2',3},{1.5,2},{2147483648,0}}) do
  assert(not pcall(h.hello_add,table.unpack(args)))
end
assert(not pcall(h.hello_greet,'a'..string.char(0)..'b'))
local d=require('dogfooding')
assert(d.xbind_version()=='0.1.0')
assert(d.xbind_target_count()==9)
assert(d.xbind_status_string(0)=='ok')
assert(type(d.xbind_find_target('Lua'))=='userdata')
assert(d.xbind_target_at(999)==nil)
assert(not pcall(d.xbind_target_at,-1))
'''
        env["LUA_CPATH"] = str(self.path / "?.so")
        self.good(run([lua, "-e", script], env=env))

    def test_tourchstone_lua_bindings_execute(self):
        if not ARGS.native_lib:
            self.skipTest("native Tourchstone libraries not supplied in standalone XBind build")
        if os.name == "nt":
            self.skipTest("generated-module harness currently uses POSIX compiler flags")
        pkg = shutil.which("pkg-config")
        if not pkg:
            self.skipTest("pkg-config absent")
        sdk = run([pkg, "--cflags", "--libs", "lua5.4"])
        if sdk.returncode:
            self.skipTest("Lua 5.4 development SDK absent")
        flags = shlex.split(sdk.stdout)
        native_flags = shlex.split(ARGS.native_flags)
        rpaths = ["-Wl,-rpath," + path for path in sorted({str(Path(lib).resolve().parent) for lib in ARGS.native_lib})]
        env = dict(os.environ)
        env["PYTHONPATH"] = str(ARGS.root / "dombind/domqlib") + os.pathsep + env.get("PYTHONPATH", "")
        self.good(run([sys.executable, "-B", ARGS.root / "dombind/domluabind/generate.py",
                       self.path / "domluabind/capi.h"], env=env))
        for relative, module in [("dynload/DynLOAD.xbind", "dynload"),
                                 ("dyninvoke/DynINVOKE.xbind", "dyninvoke"),
                                 ("Tourchstone.xbind", "tourchstone")]:
            spec = ARGS.root.parent / relative
            glue = self.path / (module + ".cpp")
            glue.write_text(self.good(self.cli("--emit-lua", spec)), encoding="utf-8")
            self.good(run([ARGS.cxx, "-std=c++20", "-shared", "-fPIC", "-Wall", "-Wextra", "-Werror",
                           *native_flags, "-I" + str(self.path), "-I" + str(spec.parent), glue,
                           *ARGS.native_lib, *rpaths, *flags, "-o", self.path / (module + ".so")]))
        script = "package.cpath=" + json.dumps(str(self.path / "?.so")) + "\n"
        script += "local fixture=" + json.dumps(ARGS.fixture) + "\n"
        script += '''local d=require('dynload')
local env=assert(d.tdyn_env_new())
assert(d.tdyn_env_add_search_path(env,'.')==0)
assert(d.tdyn_env_search_path_count(env)==1)
assert(d.tdyn_env_search_path(env,0)=='.')
assert(d.tdyn_env_library_count(env)==0)
d.tdyn_env_destroy(env)
local library=assert(d.tdyn_library_load(fixture,256))
assert(d.tdyn_library_is_inspected(library)==1)
assert(d.tdyn_library_is_native_loaded(library)==0)
assert(d.tdyn_library_section_count(library)>0)
assert(d.tdyn_library_symbol_count(library,2)>0)
d.tdyn_library_close(library)
assert(d.tdyn_library_load('no-such-library-12345',256)==nil)
assert(#d.tdyn_last_error()>0)
d.tdyn_library_close(nil)
local i=require('dyninvoke')
local int=assert(i.dinv_type_named('i32'))
assert(i.dinv_type_size(int)==4)
assert(i.dinv_type_align(int)==4)
assert(i.dinv_abi_can_execute(i.dinv_abi_host())==1)
assert(i.dinv_abi_can_execute(999)==0)
assert(i.dinv_type_named('no-such-type')==nil)
assert(#i.dinv_last_error()>0)
i.dinv_cif_destroy(nil)
local t=require('tourchstone')
assert(t.xbind_target_count()==9)
assert(t.dinv_type_size(t.dinv_type_named('f64'))==8)
assert(t.tdyn_status_string(0)=='ok')
'''
        # Embed Lua in a runner so sanitizer builds link their runtime before
        # loading instrumented native modules; no LD_PRELOAD manipulation.
        runner = self.path / "runner.cpp"
        runner.write_text('''#include <cstdio>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
int main() {
  lua_State* L=luaL_newstate(); if (!L) return 2;
  luaL_openlibs(L);
  int status=luaL_dostring(L,R"XBIND(''' + script + ''')XBIND");
  if(status) std::fprintf(stderr,"%s\\n",lua_tostring(L,-1));
  lua_close(L); return status ? 1 : 0;
}
''', encoding="utf-8")
        executable = self.path / "runner"
        self.good(run([ARGS.cxx, "-std=c++20", *native_flags, runner, *flags, "-o", executable]))
        self.good(run([executable]))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--cli", required=True)
    parser.add_argument("--cc", required=True)
    parser.add_argument("--cxx", required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--native-lib", action="append", default=[])
    parser.add_argument("--native-flags", default="")
    parser.add_argument("--fixture", default="")
    ARGS, remaining = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remaining])
