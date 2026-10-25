"""Supply isolated temporary fixtures for the Lua runtime/loader tests."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
env = {**os.environ, 'METAIL_HOME': str(root)}
scratch = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
with tempfile.TemporaryDirectory(prefix='metail-', dir=scratch) as tmp:
    # Recursive directory scanning must not follow symlink cycles.
    os.symlink(tmp, Path(tmp)/'cycle')
    subprocess.run([sys.argv[1], 'lua', str(root/'tests/test_runtime.lua'), tmp], env=env, check=True)
