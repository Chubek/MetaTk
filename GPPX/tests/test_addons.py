"""Exercise installation paths and the installed Vim syntax/indent runtime."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='gppx vim ') as tmp:
    prefix = Path(tmp) / 'runtime with spaces'
    prefix.mkdir()
    vimrc = prefix / 'vimrc'
    vimrc.write_text('" preserve user settings\n', encoding='utf-8')
    command = ['sh', str(root / 'addons/install.sh'), '--prefix', str(prefix)]
    for _ in range(2):
        subprocess.run(command, check=True, capture_output=True, text=True)
    if vimrc.read_text(encoding='utf-8') != '" preserve user settings\n':
        raise RuntimeError('installer modified user settings')
    bad = subprocess.run(['sh', str(root / 'addons/install.sh'), '--bad-option'],
                         capture_output=True, text=True)
    if bad.returncode != 2:
        raise RuntimeError('installer did not reject invalid arguments')
    env = dict(os.environ, GPPX_VIM_RUNTIME=str(prefix))
    env.pop("VIM", None)
    env.pop("VIMRUNTIME", None)
    result = subprocess.run([sys.argv[1], '-Nu', 'NONE', '-i', 'NONE', '-n', '-es',
                             '-V1', '-S', str(root / 'tests/test_vim.vim')],
                            env=env, capture_output=True, text=True)
    if result.returncode:
        sys.stderr.write(result.stdout + result.stderr)
        sys.exit(result.returncode)
print('Installed Vim runtime: file detection, highlighting and indentation pass.')
