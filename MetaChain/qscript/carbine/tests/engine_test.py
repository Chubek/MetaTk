#!/usr/bin/env python3
"""Tests for the Carbine QScript engine (source/*.qs).

The engine runs under tests/qsi.py, with the carbine:: natives replaced by
Python fakes that model a project: targets, dependency edges, file
modification times and build outcomes. The fakes print in the same format
as tooling/natives.c, so the expected output here is what the real host
prints. The C natives themselves are covered by tests/native_test.sh.

Run: python3 tests/engine_test.py    (from the project root)
"""

import contextlib
import io
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import qsi  # noqa: E402

ENGINE = os.path.join(os.path.dirname(HERE), "source")
SHELL = os.path.join(ENGINE, "shell.qs")

CMD = {"build": 1, "graph": 2, "check": 3, "lint": 4, "gen": 5, "dist": 6, "fetch": 7, "version": 8}
BACKEND = {"ninja": 1, "make": 2, "cmake": 3, "meson": 4}
OPT = {"keep_going": 1, "dry_run": 2, "verbose": 3}


class Project:
    """A fake project. Targets are dicts with name, kind, deps, out and inp
    mtimes; `fail` names targets whose build returns a nonzero status."""

    def __init__(self, targets, command, args=(), keep_going=False, fail=(), lint_issues=0):
        self.targets = targets
        self.index = {t["name"]: i for i, t in enumerate(targets)}
        self.command = command
        self.args = list(args)
        self.keep_going = keep_going
        self.fail = set(fail)
        self.lint_issues = lint_issues
        self.built = []          # names, in the order build_target ran
        self.emitted = []        # backend codes
        self.arrays = []         # index 0 unused: handles start at 1
        self.stdout = io.StringIO()
        self.stderr = io.StringIO()

    # ---- natives ----
    def natives(self):
        n = {}

        def add(name, arity, fn):
            n["carbine::" + name] = (arity, fn)

        def arr_new(a):
            self.arrays.append([0] * max(a[0], 0))
            return len(self.arrays)

        def arr(h):
            if h <= 0 or h > len(self.arrays) or self.arrays[h - 1] is None:
                raise qsi.QSError("bad array handle")
            return self.arrays[h - 1]

        def arr_get(a):
            v = arr(a[0])
            if not 0 <= a[1] < len(v):
                raise qsi.QSError("array index out of range")
            return v[a[1]]

        def arr_set(a):
            v = arr(a[0])
            if not 0 <= a[1] < len(v):
                raise qsi.QSError("array index out of range")
            v[a[1]] = a[2]
            return a[2]

        def arr_free(a):
            arr(a[0])
            self.arrays[a[0] - 1] = None
            return 0

        def target(i):
            if not 0 <= i < len(self.targets):
                raise qsi.QSError(f"target index {i} out of range")
            return self.targets[i]

        add("arr_new", 1, arr_new)
        add("arr_get", 2, arr_get)
        add("arr_set", 3, arr_set)
        add("arr_len", 1, lambda a: len(arr(a[0])))
        add("arr_free", 1, arr_free)
        add("command", 0, lambda a: CMD[self.command])
        add("gen_backend", 0, lambda a: BACKEND[self.command_backend])
        add("option", 1, lambda a: 1 if (a[0] == OPT["keep_going"] and self.keep_going) else 0)
        add("arg_count", 0, lambda a: len(self.args))
        add("arg_target", 1, lambda a: self.index.get(self.args[a[0]], -1))
        add("unknown_arg", 1, lambda a: self.stderr.write(f"carbine: no target named {self.args[a[0]]}\n") and 0)
        add("spec_load", 0, lambda a: len(self.targets))
        add("target_ndeps", 1, lambda a: len(target(a[0])["deps"]))
        add("target_dep", 2, lambda a: self.index[target(a[0])["deps"][a[1]]])
        add("show_target", 1, lambda a: self.out(f"{target(a[0])['name']}\n"))
        add("show_edge", 2, lambda a: self.out(
            f"  {target(a[0])['name']} -> {target(a[0])['deps'][a[1]]}\n"))
        add("target_input_mtime", 1, lambda a: target(a[0])["inp"])
        add("target_output_mtime", 1, lambda a: target(a[0])["out"])

        def build(a):
            name = target(a[0])["name"]
            self.built.append(name)
            return 1 if name in self.fail else 0

        add("build_target", 1, build)

        def say(a):
            label = {1: "building", 2: "up to date", 3: "failed", 4: "skipped"}[a[0]]
            self.out(f"{label}: {target(a[1])['name']}\n")
            return 0

        add("status", 2, say)

        def message(a):
            text = {1: "dependency cycle involving", 2: "build failed", 3: "build finished",
                    4: "Carbinefile is valid", 5: "lint issues found"}[a[0]]
            if a[1] >= 0:
                self.out(f"carbine: {text}: {target(a[1])['name']}\n")
            else:
                self.out(f"carbine: {text}\n")
            return 0

        add("message", 2, message)

        def emit(a):
            self.emitted.append(a[0])
            return 0

        add("emit", 1, emit)
        add("lint", 0, lambda a: self.lint_issues)
        add("dist", 0, lambda a: 0)
        add("fetch", 0, lambda a: 0)
        add("version", 0, lambda a: 0)
        add("module_load", 0, lambda a: 0)
        return n

    command_backend = "ninja"

    def out(self, text):
        self.stdout.write(text)
        return 0

    def run(self):
        """Runs the engine. Returns (exit status, stdout text)."""
        natives = self.natives()
        with contextlib.redirect_stdout(self.stdout), contextlib.redirect_stderr(self.stderr):
            status, _ = qsi.run_file(SHELL, [ENGINE], natives, out=self.stdout)
        return status, self.stdout.getvalue()


def chain_project(**kw):
    """core <- libA, libB <- app (app depends on both libraries). No outputs
    exist yet, so a build has to run every target."""
    targets = [
        {"name": "core", "deps": [], "inp": 50, "out": 0},
        {"name": "libA", "deps": ["core"], "inp": 50, "out": 0},
        {"name": "libB", "deps": ["core"], "inp": 50, "out": 0},
        {"name": "app", "deps": ["libA", "libB"], "inp": 50, "out": 0},
    ]
    return Project(targets, **kw)


class GraphTests(unittest.TestCase):
    def test_graph_lists_dependencies_first(self):
        status, text = chain_project(command="graph").run()
        self.assertEqual(status, 0)
        lines = text.splitlines()
        self.assertEqual(lines[0], "core")
        self.assertLess(lines.index("libA"), lines.index("app"))
        self.assertLess(lines.index("libB"), lines.index("app"))
        self.assertIn("  app -> libA", lines)
        self.assertIn("  app -> libB", lines)

    def test_cycle_is_reported_and_refused(self):
        targets = [
            {"name": "a", "deps": ["b"], "inp": 1, "out": 2},
            {"name": "b", "deps": ["a"], "inp": 1, "out": 2},
            {"name": "c", "deps": [], "inp": 1, "out": 2},
        ]
        status, text = Project(targets, command="check").run()
        self.assertEqual(status, 1)
        self.assertIn("carbine: dependency cycle involving: a", text)
        self.assertIn("carbine: dependency cycle involving: b", text)
        self.assertNotIn("involving: c", text)

    def test_check_accepts_valid_graph(self):
        status, text = chain_project(command="check").run()
        self.assertEqual(status, 0)
        self.assertIn("carbine: Carbinefile is valid", text)


class BuildTests(unittest.TestCase):
    def test_full_build_runs_in_dependency_order(self):
        p = chain_project(command="build")
        status, text = p.run()
        self.assertEqual(status, 0)
        self.assertEqual(p.built, ["core", "libA", "libB", "app"])
        self.assertIn("carbine: build finished", text)

    def test_up_to_date_targets_are_not_rebuilt(self):
        targets = [
            {"name": "core", "deps": [], "inp": 50, "out": 100},
            {"name": "app", "deps": ["core"], "inp": 60, "out": 200},
        ]
        p = Project(targets, command="build")
        status, text = p.run()
        self.assertEqual(status, 0)
        self.assertEqual(p.built, [])
        self.assertIn("up to date: core", text)
        self.assertIn("up to date: app", text)

    def test_newer_dependency_forces_dependents(self):
        # libA's source is newer than its output, so libA rebuilds; app is
        # up to date by its own timestamps but must rebuild because libA did.
        targets = [
            {"name": "core", "deps": [], "inp": 50, "out": 100},
            {"name": "libA", "deps": ["core"], "inp": 300, "out": 100},
            {"name": "app", "deps": ["libA"], "inp": 50, "out": 400},
        ]
        p = Project(targets, command="build")
        status, _ = p.run()
        self.assertEqual(status, 0)
        self.assertEqual(p.built, ["libA", "app"])

    def test_missing_output_is_built(self):
        targets = [{"name": "core", "deps": [], "inp": 50, "out": 0}]
        p = Project(targets, command="build")
        p.run()
        self.assertEqual(p.built, ["core"])

    def test_selected_target_builds_only_what_it_needs(self):
        p = chain_project(command="build", args=["libB"])
        status, _ = p.run()
        self.assertEqual(status, 0)
        self.assertEqual(p.built, ["core", "libB"])

    def test_unknown_target_is_an_error(self):
        p = chain_project(command="build", args=["nope"])
        status, _ = p.run()
        self.assertEqual(status, 2)
        self.assertIn("no target named nope", p.stderr.getvalue())
        self.assertEqual(p.built, [])

    def test_failure_stops_without_keep_going(self):
        p = chain_project(command="build", fail=["libA"])
        status, text = p.run()
        self.assertEqual(status, 1)
        self.assertEqual(p.built, ["core", "libA"])
        self.assertIn("carbine: build failed: libA", text)

    def test_keep_going_skips_dependents_only(self):
        p = chain_project(command="build", fail=["libA"], keep_going=True)
        status, text = p.run()
        self.assertEqual(status, 1)
        self.assertEqual(p.built, ["core", "libA", "libB"])
        self.assertIn("skipped: app", text)
        self.assertNotIn("building: app", text)

    def test_failed_dependency_does_not_fail_unrelated_targets(self):
        targets = [
            {"name": "core", "deps": [], "inp": 1, "out": 0},
            {"name": "broken", "deps": ["core"], "inp": 1, "out": 0},
            {"name": "other", "deps": [], "inp": 1, "out": 0},
        ]
        p = Project(targets, command="build", fail=["broken"], keep_going=True)
        status, _ = p.run()
        self.assertEqual(status, 1)
        self.assertIn("other", p.built)


class OtherCommandTests(unittest.TestCase):
    def test_gen_emits_selected_backend(self):
        p = chain_project(command="gen")
        p.command_backend = "cmake"
        status, _ = p.run()
        self.assertEqual(status, 0)
        self.assertEqual(p.emitted, [BACKEND["cmake"]])

    def test_gen_refuses_cycles(self):
        targets = [
            {"name": "a", "deps": ["a"], "inp": 1, "out": 2},
        ]
        p = Project(targets, command="gen")
        status, _ = p.run()
        self.assertEqual(status, 1)
        self.assertEqual(p.emitted, [])

    def test_lint_fails_on_issues(self):
        status, text = chain_project(command="lint", lint_issues=3).run()
        self.assertEqual(status, 1)
        self.assertIn("carbine: lint issues found", text)

    def test_lint_passes_clean_tree(self):
        status, _ = chain_project(command="lint").run()
        self.assertEqual(status, 0)

    def test_version_and_dist_and_fetch_return_zero(self):
        for command in ("version", "dist", "fetch"):
            status, _ = Project([], command=command, args=["a", "b"]).run()
            self.assertEqual(status, 0, command)


class LibraryTests(unittest.TestCase):
    """The depgraph algorithms directly, through a tiny driver program."""

    def run_driver(self, body, targets):
        import tempfile
        src = "use depgraph, ffi;\n" + body
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "driver.qs")
            with open(path, "w") as f:
                f.write(src)
            p = Project(targets, command="graph")
            natives = p.natives()
            with contextlib.redirect_stdout(p.stdout):
                result, _ = qsi.run_file(path, [ENGINE], natives, out=p.stdout)
        return result

    def test_closure_marks_transitive_dependencies(self):
        targets = [
            {"name": "app", "deps": ["lib"], "inp": 0, "out": 0},
            {"name": "lib", "deps": ["core"], "inp": 0, "out": 0},
            {"name": "core", "deps": [], "inp": 0, "out": 0},
            {"name": "unrelated", "deps": [], "inp": 0, "out": 0},
        ]
        result = self.run_driver(
            """
let mark = vec_new(4);
vec_fill(mark, 0);
vec_set(mark, 0, 1);
close_over_deps(4, mark);
return vec_get(mark, 0) * 1000 + vec_get(mark, 1) * 100 + vec_get(mark, 2) * 10 + vec_get(mark, 3);
""",
            targets,
        )
        self.assertEqual(result, 1110)

    def test_topo_order_places_all_targets_of_a_dag(self):
        targets = [
            {"name": "app", "deps": ["lib", "core"], "inp": 0, "out": 0},
            {"name": "lib", "deps": ["core"], "inp": 0, "out": 0},
            {"name": "core", "deps": [], "inp": 0, "out": 0},
        ]
        result = self.run_driver(
            """
let order = vec_new(3);
let pending = vec_new(3);
let placed = topo_order(3, order, pending);
// core (2) must come before lib (1), which must come before app (0).
let ok = 0;
if (placed == 3 && vec_get(order, 0) == 2 && vec_get(order, 1) == 1 && vec_get(order, 2) == 0) ok = 1;
return ok;
""",
            targets,
        )
        self.assertEqual(result, 1)

    def test_topo_order_detects_cycle(self):
        targets = [
            {"name": "a", "deps": ["b"], "inp": 0, "out": 0},
            {"name": "b", "deps": ["a"], "inp": 0, "out": 0},
            {"name": "free", "deps": [], "inp": 0, "out": 0},
        ]
        result = self.run_driver(
            """
let order = vec_new(3);
let pending = vec_new(3);
return topo_order(3, order, pending);
""",
            targets,
        )
        self.assertEqual(result, 1)   # only "free" can be placed


if __name__ == "__main__":
    unittest.main(verbosity=2)
