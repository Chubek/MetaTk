"""Structured APIs must work when the textual program parser is unavailable."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
scratch = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
with tempfile.TemporaryDirectory(prefix='metail-parser-free-', dir=scratch) as temporary:
    home = Path(temporary)
    shutil.copytree(root / 'lua', home / 'lua')
    (home / 'optimize-kernels').symlink_to(root / 'optimize-kernels', target_is_directory=True)
    api = home / 'lua/metail/api.lua'
    api.write_text("require('metail.parser').program = function() error('TEXT PARSER DISABLED') end\n" + api.read_text())
    for executable in sys.argv[1:]:
        subprocess.run([executable, str(home), 'parser-disabled'], check=True)
