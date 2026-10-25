"""Evaluate C++ DSLtk rewrite results with MetaIL's signed-64 Lua evaluator."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
result = subprocess.run([sys.argv[1], str(root)], check=True, stdout=subprocess.PIPE)
with tempfile.TemporaryDirectory(prefix='metail-dsl-semantics-') as temporary:
    corpus = Path(temporary) / 'pairs.mil'
    corpus.write_bytes(result.stdout)
    subprocess.run([sys.argv[2], 'lua', str(root / 'tests/dsl_semantics.lua'),
                    str(corpus)], check=True)
