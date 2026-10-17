"""The QScript build rejects unowned and duplicate implementation sources."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
manifest = (ROOT / "qscript/cmake/Sources.cmake").read_text()
sources = re.findall(r"qscript/src/[A-Za-z0-9]+\.cpp", manifest)
assert len(sources) == len(set(sources)), "source has multiple owners"
assert set(sources) == {p.relative_to(ROOT).as_posix() for p in (ROOT / "qscript/src").glob("*.cpp")}
scratch = "/tmp/opencode" if Path("/tmp/opencode").is_dir() else None
with tempfile.TemporaryDirectory(prefix="qscript-strata-", dir=scratch) as directory:
    tmp = Path(directory)
    (tmp / "qscript/src").mkdir(parents=True)
    for name in sources:
        (tmp / name).touch()
    selected = tmp / "Sources.cmake"
    selected.write_text(manifest)
    (tmp / "CMakeLists.txt").write_text('cmake_minimum_required(VERSION 3.20)\nproject(Strata NONE)\ninclude(Sources.cmake)\n')

    def configure():
        return subprocess.run(["cmake", "-S", str(tmp), "-B", str(tmp / "build")],
                              capture_output=True, text=True, timeout=30)

    proc = configure()
    assert proc.returncode == 0, proc.stdout + proc.stderr
    unowned = tmp / "qscript/src/Unowned.cpp"
    unowned.touch()
    proc = configure()
    assert proc.returncode != 0 and "strata are incomplete or duplicate" in proc.stderr
    unowned.unlink()
    selected.write_text(manifest.replace("set(QSCRIPT_SOURCE_OWNERS ${QSCRIPT_SOURCES})",
                                        "set(QSCRIPT_SOURCE_OWNERS ${QSCRIPT_SOURCES} qscript/src/Runtime.cpp)"))
    proc = configure()
    assert proc.returncode != 0 and "strata are incomplete or duplicate" in proc.stderr

print("QScript source ownership and negative configuration checks passed")
