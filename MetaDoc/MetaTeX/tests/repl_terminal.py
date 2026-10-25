#!/usr/bin/env python3
"""Real PTY smoke test for replxx editing, completion, prompt and history."""
import os
import pathlib
import pty
import re
import select
import subprocess
import sys
import tempfile
import time

with tempfile.TemporaryDirectory() as td:
    master, slave = pty.openpty()
    env = dict(os.environ, TERM='xterm-256color', XDG_STATE_HOME=td)
    process = subprocess.Popen([sys.argv[1], '-f', 'plain', '-i'], stdin=slave, stdout=slave,
                               stderr=slave, env=env, start_new_session=True)
    os.close(slave)
    transcript = bytearray()
    ansi = re.compile(rb'\x1b\[[0-?]*[ -/]*[@-~]')
    cursor = 0

    def wait_for(marker):
        global cursor
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            clean = ansi.sub(b'', transcript)
            found = clean.find(marker, cursor)
            if found >= 0:
                cursor = found + len(marker)
                return
            if select.select([master], [], [], 0.1)[0]:
                data = os.read(master, 65536)
                transcript.extend(data)
                if b'\x1b[6n' in data:
                    os.write(master, b'\x1b[1;1R')
            if process.poll() is not None:
                break
        raise AssertionError((marker, bytes(transcript)))

    try:
        wait_for(b'mtex> ')
        os.write(master, b'\\def\\word{kept}\r')
        wait_for(b'mtex> ')
        os.write(master, b'\\word\r')
        wait_for(b'kept\r\n\r\n')
        wait_for(b'mtex> ')
        # Complete a real engine command, then evaluate it.
        os.write(master, b'\\textb\t{TTY}\r')
        wait_for(b'TTY\r\n\r\n')
        wait_for(b'mtex> ')
        os.write(master, b':quit\r')
        assert process.wait(timeout=10) == 0
        history = pathlib.Path(td, 'metatex/mtex.history').read_text()
        assert '\\textbf{TTY}' in history and '\\word' in history
        assert b'undefined control sequence' not in transcript
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        os.close(master)
print('replxx terminal test passed')
