#!/usr/bin/env python3
"""Scaffold checks for XBind.

These tests intentionally avoid optional target SDKs. They verify that the tree is
coherent enough for the real parser and generators to be added incrementally.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
import unittest
from pathlib import Path


TARGETS = {
    "D": ("compilers/D.c", "dombind/domdbind", "typedefs/D-Default.sexpr"),
    "Go": ("compilers/Go.c", "dombind/domgobind", "typedefs/Go-Default.sexpr"),
    "Java": ("compilers/Java.c", "dombind/domjavabind", "typedefs/Java-Default.sexpr"),
    "Lua": ("compilers/Lua.c", "dombind/domluabind", "typedefs/Lua-Default.sexpr"),
    "NodeJS": ("compilers/NodeJS.c", "dombind/domnodebind", "typedefs/NodeJs-Default.sexpr"),
    "OCaml": ("compilers/OCaml.c", "dombind/domocamlbind", "typedefs/OCaml-Default.sexpr"),
    "Python": ("compilers/Python.c", "dombind/dompybind", "typedefs/Python-Default.sexpr"),
    "Ruby": ("compilers/Ruby.c", "dombind/domrubybind", "typedefs/Ruby-Default.sexpr"),
    "Rust": ("compilers/Rust.c", "dombind/domrustbind", "typedefs/Rust-Default.sexpr"),
}

TARGET_DUMP_LINE = "targets: " + " ".join(TARGETS)

REPOSITORY_SPECS = {
    "yellowstone/Yellowstone.xbind": "yellowstone",
    "sandstone/Sandstone.xbind": "sandstone",
    "limestone/Limestone.xbind": "limestone",
    "blackstone/Blackstone.xbind": "blackstone",
    "firestone/Firestone.xbind": "firestone",
    "tourchstone/Tourchstone.xbind": "tourchstone",
    "tourchstone/dyninvoke/DynINVOKE.xbind": "dyninvoke",
    "tourchstone/dynload/DynLOAD.xbind": "dynload",
    "tourchstone/xbind/Dogfooding.xbind": "dogfooding",
    "tourchstone/xbind/examples/hello/hello.xbind": "hello",
}

# Jiterati is a gitlink and may be absent in a non-recursive checkout.
if (Path(__file__).resolve().parents[3] / "jiterati/Jiterati.xbind").is_file():
    REPOSITORY_SPECS["jiterati/Jiterati.xbind"] = "jiterati"


def balanced_sexpr(text: str) -> bool:
    depth = 0
    in_string = False
    escaped = False
    for char in text:
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
        elif char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth < 0:
                return False
    return depth == 0 and not in_string


def repo_root(root: Path) -> Path:
    # root is normally <repo>/tourchstone/xbind.
    return root.parents[1]


def lua_string(value: str) -> str:
    return json.dumps(value)


def spec_string_set(pattern: str, text: str) -> set[str]:
    return set(re.findall(pattern, text))


class XBindRegimen(unittest.TestCase):
    root: Path
    cli: str | None

    def test_required_roots_exist(self) -> None:
        for relative in ["lxbind", "stdlib", "typedefs", "compilers", "dombind", "examples"]:
            self.assertTrue((self.root / relative).is_dir(), relative)
        for relative in ["xbind.c", "xbind.h", "xbind.hpp", "XFeats.schema.json", "README.md", "REGIMEN.md"]:
            self.assertTrue((self.root / relative).is_file(), relative)

    def test_xfeats_schema_is_json(self) -> None:
        with (self.root / "XFeats.schema.json").open("r", encoding="utf-8") as handle:
            schema = json.load(handle)
        self.assertEqual(schema.get("title"), "XFeats Manifest Schema")
        self.assertIn("xfeats", schema.get("properties", {}))

    def test_targets_have_scaffold_paths(self) -> None:
        for target, paths in TARGETS.items():
            with self.subTest(target=target):
                compiler, dombind, typedefs = paths
                self.assertTrue((self.root / compiler).is_dir(), compiler)
                self.assertTrue((self.root / dombind).is_dir(), dombind)
                self.assertTrue((self.root / typedefs).is_file(), typedefs)

    def test_typedef_maps_are_nonempty_balanced_sexprs(self) -> None:
        for target, (_, _, typedefs) in TARGETS.items():
            with self.subTest(target=target):
                text = (self.root / typedefs).read_text(encoding="utf-8")
                self.assertIn("xbind-typedefs", text)
                self.assertIn(f'(target "{target}")', text)
                self.assertTrue(balanced_sexpr(text), typedefs)

    def test_lxbind_lua_exposes_spec_constructors(self) -> None:
        text = (self.root / "lxbind" / "lxbind.lua").read_text(encoding="utf-8")
        for token in ["function M.library", "function M.target", "function M.typedef", "function M.func", "return M"]:
            self.assertIn(token, text)

    def test_hello_fixture_has_spec_and_header(self) -> None:
        fixture = self.root / "examples" / "hello"
        spec = (fixture / "hello.xbind").read_text(encoding="utf-8")
        header = (fixture / "hello.h").read_text(encoding="utf-8")
        self.assertIn('name = "hello"', spec)
        self.assertIn('xbind.func("hello_add"', spec)
        self.assertIn("int hello_add", header)

    def test_repository_specs_have_all_targets_and_headers(self) -> None:
        repo = repo_root(self.root)
        for relative, library in REPOSITORY_SPECS.items():
            with self.subTest(spec=relative):
                path = repo / relative
                text = path.read_text(encoding="utf-8")
                self.assertIn(f'name = "{library}"', text)
                for target in TARGETS:
                    self.assertIn(f'xbind.target("{target}"', text)

                header_match = re.search(r"headers\s*=\s*\{([^}]*)\}", text, re.S)
                self.assertIsNotNone(header_match, relative)
                headers = re.findall(r'"([^"]+)"', header_match.group(1))
                self.assertTrue(headers, relative)
                for header in headers:
                    self.assertTrue((path.parent / header).is_file(), header)

    def test_repository_spec_functions_match_headers(self) -> None:
        repo = repo_root(self.root)
        for relative in REPOSITORY_SPECS:
            with self.subTest(spec=relative):
                path = repo / relative
                text = path.read_text(encoding="utf-8")
                functions = re.findall(r'xbind\.func\("([^"]+)"', text)
                self.assertEqual(len(functions), len(set(functions)))
                header_match = re.search(r"headers\s*=\s*\{([^}]*)\}", text, re.S)
                headers = re.findall(r'"([^"]+)"', header_match.group(1)) if header_match else []
                header_text = "\n".join(
                    (path.parent / header).read_text(encoding="utf-8", errors="ignore")
                    for header in headers
                )
                for function in functions:
                    self.assertIn(function, header_text, function)

    def test_repository_spec_types_are_mapped_or_overridden(self) -> None:
        default_types: set[str] = set()
        for typedefs in (self.root / "typedefs").glob("*-Default.sexpr"):
            default_types.update(
                re.findall(r'\(c "([^"]+)"\)', typedefs.read_text(encoding="utf-8"))
            )
        self.assertTrue(default_types)

        repo = repo_root(self.root)
        for relative in REPOSITORY_SPECS:
            with self.subTest(spec=relative):
                text = (repo / relative).read_text(encoding="utf-8")
                local_types = spec_string_set(r'xbind\.typedef\("([^"]+)"', text)
                used_types = spec_string_set(r'returns\s*=\s*"([^"]+)"', text)
                used_types.update(
                    re.findall(r'xbind\.param\("[^"]+",\s*"([^"]+)"', text)
                )
                unmapped = used_types - default_types - local_types
                self.assertFalse(unmapped, sorted(unmapped))

    def test_repository_specs_load_with_stock_lua(self) -> None:
        lua = shutil.which("lua5.4") or shutil.which("lua")
        if not lua:
            self.skipTest("no stock Lua interpreter found")
        repo = repo_root(self.root)
        for relative, library in REPOSITORY_SPECS.items():
            with self.subTest(spec=relative):
                path = repo / relative
                script = (
                    "package.path="
                    + lua_string(str(self.root / "lxbind" / "?.lua") + ";")
                    + "..package.path; "
                    + "local x=require('lxbind'); "
                    + "local m=assert(loadfile("
                    + lua_string(str(path))
                    + "))(); "
                    + "x.validate_library(m); "
                    + "assert(m.name=="
                    + lua_string(library)
                    + "); "
                    + "assert(#m.targets==9); "
                    + "print(m.name)"
                )
                subprocess.run([lua, "-e", script], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def test_cli_smoke(self) -> None:
        if not self.cli:
            self.skipTest("no --cli provided")
        list_result = subprocess.run(
            [self.cli, "--list-targets"],
            check=True,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        listed = set(list_result.stdout.split())
        self.assertEqual(set(TARGETS), listed)
        subprocess.run([self.cli, "--check-layout", str(self.root)], check=True)
        spec_result = subprocess.run(
            [self.cli, "--find-spec", "hello", str(self.root / "examples" / "hello")],
            check=True,
            text=True,
            stdout=subprocess.PIPE,
        )
        self.assertTrue(spec_result.stdout.strip().endswith("hello.xbind"))
        dump_result = subprocess.run(
            [self.cli, "--dump-spec", "hello", str(self.root / "examples" / "hello")],
            check=True,
            text=True,
            stdout=subprocess.PIPE,
        )
        self.assertIn("library: hello", dump_result.stdout)
        self.assertIn(TARGET_DUMP_LINE, dump_result.stdout)
        self.assertIn("hello_add -> int", dump_result.stdout)
        dogfood_result = subprocess.run(
            [self.cli, "--dump-spec", "Dogfooding", str(self.root)],
            check=True,
            text=True,
            stdout=subprocess.PIPE,
        )
        self.assertIn("library: dogfooding", dogfood_result.stdout)
        self.assertIn(TARGET_DUMP_LINE, dogfood_result.stdout)
        self.assertIn("xbind_version -> const char *", dogfood_result.stdout)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--cli")
    args, remaining = parser.parse_known_args(argv)
    XBindRegimen.root = args.root.resolve()
    XBindRegimen.cli = args.cli
    unittest.main(argv=[sys.argv[0], *remaining])
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
