"""Validate the Juniper manual and its runnable Markdown examples.

No third-party Python packages are needed. C++ examples are extracted verbatim
and built as a separate source consumer when --cpp is supplied.
"""

import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
from urllib.parse import unquote


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def run(command, environment, expected=0, timeout=60):
    result = subprocess.run(list(map(str, command)), env=environment,
                            capture_output=True, text=True, timeout=timeout)
    require(result.returncode == expected,
            f"command returned {result.returncode}, expected {expected}: "
            f"{shlex.join(list(map(str, command)))}\n{result.stdout}\n{result.stderr}")
    return result


def blocks(path):
    """Yield language, code, check metadata, and opening-fence line number."""
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    pending = None
    index = 0
    while index < len(lines):
        annotation = re.fullmatch(r"<!-- check: (.*?) -->\s*", lines[index])
        if annotation:
            words = shlex.split(annotation.group(1))
            require(bool(words), f"empty check annotation in {path}")
            pending = {"kind": words[0]}
            for word in words[1:]:
                key, separator, value = word.partition("=")
                pending[key] = value if separator else True
            index += 1
            continue
        if lines[index].startswith("```"):
            language = lines[index][3:].strip()
            opening = index + 1
            index += 1
            code = []
            while index < len(lines) and lines[index].strip() != "```":
                code.append(lines[index])
                index += 1
            require(index < len(lines), f"unclosed fence in {path}:{opening}")
            yield language, "".join(code), pending, opening
            pending = None
        elif lines[index].strip():
            require(pending is None, f"check annotation not followed by a fence: {path}")
        index += 1
    require(pending is None, f"unused check annotation in {path}")


def check_structure(manual):
    chapters = sorted(manual.glob("[0-9][0-9]-*.md"))
    require(len(chapters) == 36, f"expected 36 chapters, found {len(chapters)}")
    require([int(path.name[:2]) for path in chapters] == list(range(1, 37)),
            "chapter numbering is not 01 through 36")
    documents = [manual / "README.md", *chapters,
                 *sorted((manual / "examples").rglob("*.md"))]
    links = 0
    fences = 0
    for path in documents:
        text = path.read_text(encoding="utf-8")
        require(text.endswith("\n"), f"missing final newline: {path}")
        # Remove code before looking for Markdown links; code is not prose.
        prose = re.sub(r"^```[^\n]*\n.*?^```[^\n]*(?:\n|$)", "", text,
                       flags=re.MULTILINE | re.DOTALL)
        for target in re.findall(r"\[[^\]\n]+\]\(([^\s)]+)\)", prose):
            if re.match(r"[a-zA-Z][a-zA-Z0-9+.-]*:", target) or target.startswith("#"):
                continue
            filename = unquote(target.split("#", 1)[0])
            require((path.parent / filename).exists(), f"broken link in {path}: {target}")
            links += 1
        for _ in blocks(path):
            fences += 1
    return chapters, documents, links, fences


def check_program(tool, environment, source, work, name, options=None):
    options = options or {}
    path = work / f"{name}.jun"
    path.write_text(source, encoding="utf-8")
    mode = options.get("mode", "check")
    require(mode in {"check", "select", "schedule", "optimize"}, f"unsupported example mode {mode}")
    flags = ["--no-preprocess"] if options.get("no-preprocess") else []
    if options.get("passes"):
        flags.extend(["--passes", options["passes"]])
    expected_error = options.get("error")
    result = run([tool, mode, path, *flags], environment, 1 if expected_error else 0)
    if expected_error:
        require(expected_error in result.stderr,
                f"{name}: expected {expected_error}, received {result.stderr}")
        return
    if mode in {"select", "schedule"}:
        machine = json.loads(result.stdout)
        opcodes = {inst["opcode"] for function in machine["functions"]
                   for inst in function["instructions"]}
        for opcode in str(options.get("opcodes", "")).split(","):
            if opcode:
                require(opcode in opcodes, f"{name}: expected selected opcode {opcode}")
        if mode == "schedule":
            require(machine["scheduled"] is True, f"{name}: not scheduled")
            require(all(inst["cycle"] is not None for function in machine["functions"]
                        for inst in function["instructions"]), f"{name}: missing cycles")
    canonical_flags = ["--no-preprocess"] if options.get("no-preprocess") else []
    canonical = (result.stdout if mode == "optimize" else
                 run([tool, "emit", path, *canonical_flags], environment).stdout)
    canonical_path = work / f"{name}.junir"
    canonical_path.write_text(canonical, encoding="utf-8")
    repeated = run([tool, "emit", canonical_path, "--no-preprocess"], environment).stdout
    require(canonical == repeated, f"{name}: canonical round trip changed output")


def check_fixtures(manual, tool, environment, work):
    examples = manual / "examples"
    inspector = examples / "inspect_schedule.py"
    files = [examples / name for name in (
        "affine.jun", "effects.jun", "loop.jun", "lazy.jun", "parallel.jun",
        "includes/Juniperfile")]
    for number, path in enumerate(files):
        run([tool, "check", path], environment)
        output = work / f"fixture-{number}.json"
        run([tool, "schedule", path, "-o", output], environment)
        run([sys.executable, inspector, output], environment)
    for passes in ("ControlFlowGraph,Dominators,LiveVariables,ReachingDefinitions",
                   "WeighstoneIntegerFacts,NumericLiteralsMean"):
        report = run([tool, "analyze", examples / "loop.jun", "--passes", passes],
                     environment)
        require(isinstance(json.loads(report.stdout), dict), "invalid analysis report")
    run([tool, "optimize", examples / "effects.jun", "--passes",
         "DeadAssignmentElimination"], environment)
    include_output = work / "includes.junir"
    depfile = work / "includes.d"
    run([tool, "emit", examples / "includes/Juniperfile", "-o", include_output,
         "--depfile", depfile], environment)
    dependencies = depfile.read_text(encoding="utf-8")
    require("constants.xpp" in dependencies and "arithmetic.xpp" in dependencies,
            "nested include dependencies missing")

    for fusion in (0, 1):
        source = work / f"capstone-{fusion}.junir"
        optimized = work / f"capstone-{fusion}.optimized.junir"
        scheduled = work / f"capstone-{fusion}.json"
        dependencies = work / f"capstone-{fusion}.d"
        run([tool, "emit", examples / "capstone/Juniperfile", f"-DUSE_FUSION={fusion}",
             "-o", source, "--depfile", dependencies], environment)
        run([tool, "optimize", source, "--no-preprocess", "--passes",
             "ConstantFolding,AddZeroRight", "-o", optimized], environment)
        run([tool, "schedule", optimized, "--no-preprocess", "-o", scheduled], environment)
        opcode = "MADD" if fusion else "MUL"
        run([sys.executable, inspector, scheduled, "--expect-opcode", opcode], environment)
        if not fusion:
            run([sys.executable, inspector, scheduled, "--forbid-opcode", "MADD"], environment)
        text = dependencies.read_text(encoding="utf-8")
        active = "fused.xpp" if fusion else "base.xpp"
        inactive = "base.xpp" if fusion else "fused.xpp"
        require(active in text and inactive not in text, "conditional depfile is inaccurate")
    return len(files) + 2


def bracket(value):
    value = str(value)
    equals = "="
    while f"]{equals}]" in value:
        equals += "="
    return f"[{equals}[{value}]{equals}]"


def check_cpp(snippets, arguments, manual, environment, work):
    source_root = arguments.source_root or manual.parent
    source_root = source_root.resolve()
    require((source_root / "CMakeLists.txt").is_file(),
            "--cpp requires a source checkout; supply --source-root when using an installed manual")
    generated = work / "extracted-cpp"
    generated.mkdir(parents=True, exist_ok=True)
    cmake = ["cmake_minimum_required(VERSION 3.24)",
             "project(JuniperManualExamples LANGUAGES C CXX)",
             "set(CMAKE_CXX_EXTENSIONS OFF)",
             "set(JUNIPER_BUILD_TESTS OFF)", "set(METAIL_BUILD_TESTS OFF)",
             "set(JUNIPER_BUILD_EXAMPLES OFF)",
             f"set(JUNIPER_WITH_METAARCH {'ON' if arguments.with_metaarch else 'OFF'})"]
    if arguments.yyjson_source:
        cmake.append(f"set(FETCHCONTENT_SOURCE_DIR_YYJSON {bracket(arguments.yyjson_source.resolve())})")
    cmake.extend([f"add_subdirectory({bracket(source_root)} dependency EXCLUDE_FROM_ALL)",
                  "enable_testing()"])
    count = 0
    for name, code, metadata in snippets:
        if metadata.get("metaarch") and not arguments.with_metaarch:
            print(f"skip optional MetaArch example: {name}")
            continue
        path = generated / f"{name}.cpp"
        path.write_text(code, encoding="utf-8")
        target = "manual_" + re.sub(r"[^a-zA-Z0-9_]", "_", name)
        library = "Juniper::MetaArch" if metadata.get("metaarch") else "Juniper::Juniper"
        cmake.extend([
            f"add_executable({target} {bracket(path)})",
            f"target_include_directories({target} PRIVATE {bracket(manual / 'examples')})",
            f"target_link_libraries({target} PRIVATE {library})",
            f"add_test(NAME {target} COMMAND {target} "
            f"{bracket(arguments.metail_home.resolve())} {bracket(source_root)})"])
        count += 1
    (generated / "CMakeLists.txt").write_text("\n".join(cmake) + "\n", encoding="utf-8")
    build = work / "cpp-build"
    run(["cmake", "-S", generated, "-B", build, "-DCMAKE_BUILD_TYPE=Release",
         "-DCMAKE_C_FLAGS=-Wall -Wextra -Wpedantic -Werror",
         "-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic -Werror"], environment, timeout=300)
    run(["cmake", "--build", build, "-j", arguments.jobs], environment, timeout=600)
    tested = run(["ctest", "--test-dir", build, "--output-on-failure", "-j", arguments.jobs],
                 environment, timeout=180)
    print(tested.stdout, end="")
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--juniper", type=Path, required=True)
    parser.add_argument("--metail-home", type=Path, required=True)
    parser.add_argument("--cpp", action="store_true")
    parser.add_argument("--with-metaarch", action="store_true")
    parser.add_argument("--source-root", type=Path)
    parser.add_argument("--yyjson-source", type=Path)
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--jobs", type=int, default=4)
    arguments = parser.parse_args()
    require(not arguments.with_metaarch or arguments.cpp, "--with-metaarch requires --cpp")
    require(arguments.jobs > 0, "--jobs must be positive")
    tool = arguments.juniper.resolve()
    require(tool.is_file(), f"Juniper executable not found: {tool}")
    manual = Path(__file__).resolve().parent
    chapters, documents, links, fences = check_structure(manual)
    environment = dict(os.environ, METAIL_HOME=str(arguments.metail_home.resolve()))
    temporary = None
    if arguments.build_dir:
        work = arguments.build_dir.resolve()
        require(work != manual and work not in manual.parents and manual not in work.parents,
                "choose a separate generated build directory outside the manual")
        work.mkdir(parents=True, exist_ok=True)
    else:
        temporary = tempfile.TemporaryDirectory(prefix="juniper-manual-")
        work = Path(temporary.name)
    try:
        programs = python = 0
        snippets = []
        for path in documents:
            for number, (language, code, metadata, line) in enumerate(blocks(path), 1):
                if metadata is None:
                    continue
                kind = metadata["kind"]
                require(language == kind, f"annotation/language mismatch in {path}:{line}")
                name = f"{path.stem}-{number}"
                if kind == "juniper":
                    check_program(tool, environment, code, work, name, metadata)
                    programs += 1
                elif kind == "cpp":
                    snippets.append((name, code, metadata))
                elif kind == "python":
                    compile(code, f"{path}:{line}", "exec")
                    python += 1
                else:
                    raise RuntimeError(f"unknown check type: {kind}")
        fixtures = check_fixtures(manual, tool, environment, work)
        cpp = check_cpp(snippets, arguments, manual, environment, work) if arguments.cpp else 0
        print(f"validated {len(chapters)} chapters, {links} local links, {fences} code blocks")
        print(f"checked {programs} inline Juniper cases, {fixtures} fixture configurations, "
              f"{python} Python snippets, {cpp} compiled/running C++ examples")
    finally:
        if temporary is not None:
            temporary.cleanup()


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"manual check failed: {error}", file=sys.stderr)
        raise SystemExit(1)
