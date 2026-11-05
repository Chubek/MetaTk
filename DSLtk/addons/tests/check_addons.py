#!/usr/bin/env python3
"""Headless Vim/Neovim integration, safe installation, and Sublime format checks."""
import argparse
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import tempfile

ADDONS = Path(__file__).resolve().parents[1]


def quote(value):
    return "'" + str(value).replace("'", "''") + "'"


def check_editor(editor, root, metaml):
    root.mkdir()
    log = root / 'arguments.jsonl'
    fake = root / 'fake metaml'
    fake.write_text('''#!/usr/bin/env python3
import json, sys
from pathlib import Path
with Path(%r).open('a') as out:
    out.write(json.dumps(sys.argv[1:]) + '\\n')
if '--check' in sys.argv:
    print(json.dumps({'file': sys.argv[-1], 'line': 2, 'column': 3,
                      'severity': 'error', 'code': 'M0301', 'message': 'test error'}))
    sys.exit(1)
print('editor command output')
''' % str(log))
    fake.chmod(0o755)
    program = root / "program 'quoted';$name.mml"
    program.write_text('answer := 42\nanswer\n')
    (root / 'MAMMAL').write_text('Mammal.project("editor-test")\n')
    (root / 'types.mmli').write_text('answer :: Int\n')
    errors = root / 'errors.txt'
    script = [
        'set nocompatible', 'set nomore', 'set shell=/bin/sh',
        'execute "set runtimepath^=" . fnameescape(' + quote(ADDONS / 'Vim') + ')',
        'filetype plugin indent on', 'syntax enable',
        'try',
    ]
    for name in ['program.mml', 'types.mmli', 'types.mlli', 'MAMMAL']:
        script += [
            'execute "edit " . fnameescape(' + quote(root / name) + ')',
            "call assert_equal('metaml', &filetype)",
            "call assert_equal('metaml', b:current_syntax)",
            "call assert_equal('-- %s', &commentstring)",
            "call assert_equal('MetaMLIndent(v:lnum)', &indentexpr)",
            "call assert_equal(2, &shiftwidth)",
            "call assert_equal(2, exists(':MetaMLCheck'))",
        ]
    samples = [
        ('module', 'metamlKeyword'), ('true', 'metamlBoolean'),
        ('0x4_f', 'metamlNumber'), ('0b10_01', 'metamlNumber'),
        ('1_024', 'metamlNumber'), ('1.2e-3', 'metamlFloat'),
        ("'element", 'metamlTypeVariable'), ('$12', 'metamlPlaceholder'),
        ('#2', 'metamlPlaceholder'), ('#help', 'metamlDirective'),
        ('%type value', 'metamlDirective'), ('-- note', 'metamlComment'),
        ('# a comment', 'metamlComment'), ('Constructor', 'metamlConstructor'),
        ('∘', 'metamlOperator'),
    ]
    for sample, group in samples:
        script += [
            'call setline(1, ' + quote(sample) + ')', 'syntax sync fromstart',
            'call assert_equal(' + quote(group) + ", synIDattr(synID(1, 1, 1), 'name'), " + quote(sample) + ')',
        ]
    syntax_lines = ['{- outer {- inner -}', 'still a comment -}',
                    'text := "not -- a comment \\n"', 'raw := r"\\n"', 'bad := "\\q"']
    script += [
        'call setline(1, "fold\'argument")', 'syntax sync fromstart',
        "call assert_notequal('metamlTypeVariable', synIDattr(synID(1, 5, 1), 'name'))",
        'silent %delete _',
        'call setline(1, json_decode(' + quote(json.dumps(syntax_lines)) + '))',
        'syntax sync fromstart',
        "call assert_equal('metamlBlockComment', synIDattr(synID(2, 1, 1), 'name'))",
        "call assert_equal('metamlString', synIDattr(synID(3, 14, 1), 'name'))",
        "call assert_equal('metamlEscape', synIDattr(synID(3, 27, 1), 'name'))",
        "call assert_equal('metamlRawString', synIDattr(synID(4, 10, 1), 'name'))",
        "call assert_equal('metamlEscapeError', synIDattr(synID(5, 9, 1), 'name'))",
        'silent %delete _',
    ]
    lines = ['module Outer := struct', 'val x := 1', 'module Inner := struct',
             'val y := 2', 'end', 'end', 'f x :=', '  x + 1', 'g y := y',
             'xs := [', '  1,', '  2', ']']
    expected = ['module Outer := struct', '  val x := 1', '  module Inner := struct',
                '    val y := 2', '  end', 'end', 'f x :=', '  x + 1', 'g y := y',
                'xs := [', '  1,', '  2', ']']
    script += [
        'call setline(1, json_decode(' + quote(json.dumps(lines)) + '))',
        'syntax sync fromstart',
        'normal! gg=G',
        'call assert_equal(json_decode(' + quote(json.dumps(expected)) + '), getline(1, "$"))',
        'setlocal nomodified',
        'execute "edit " . fnameescape(' + quote(program) + ')',
        'let g:metaml_command = ' + quote(fake),
        'let g:metaml_mammal_command = ' + quote(fake),
        'let g:metaml_options = ["--stdlib", "path with spaces"]',
        'call setline(1, "unsaved := 0")',
        'try', 'MetaMLCheck', "call assert_report('unsaved check was not refused')",
        "catch /save the buffer/", 'endtry',
        'silent edit!',
        'MetaMLCheck',
        "call assert_equal(1, len(getqflist()))",
        "call assert_equal(2, getqflist()[0].lnum)",
        "call assert_equal(3, getqflist()[0].col)",
        "call assert_equal('M0301: test error', getqflist()[0].text)",
        'cclose',
        "call metaml#Run(['one argument; $literal'])",
        "call assert_match('MetaML run (exit 0)', getline(1))",
        "call assert_equal('editor command output', getline(3))",
        'bwipeout!',
        'MetaMLAST',
        "call assert_match('MetaML AST', getline(1))",
        'bwipeout!',
        'MetaMLMammal show',
        "call assert_match('MAMMAL show', getline(1))",
        'bwipeout!',
        'execute "edit " . fnameescape(' + quote(root / 'types.mmli') + ')',
        'try', 'MetaMLRun', "call assert_report('interface execution was not refused')",
        "catch /interfaces are not runnable/", 'endtry',
        'execute "edit " . fnameescape(' + quote(root / 'MAMMAL') + ')',
        'MetaMLCheck', "call assert_match('MAMMAL check', getline(1))", 'bwipeout!',
        "call metaml#Run(['project argument'])",
        "call assert_match('MAMMAL run', getline(1))", 'bwipeout!',
        'execute "edit " . fnameescape(' + quote(root / 'plain.txt') + ')',
        "call assert_equal(0, exists(':MetaMLCheck'))",
    ]
    if metaml:
        script += [
            'execute "edit " . fnameescape(' + quote(program) + ')',
            'let g:metaml_command = ' + quote(metaml),
            "let g:metaml_options = ['--no-stdlib']",
            'MetaMLCheck', 'call assert_equal([], getqflist())',
            'call setline(1, "answer := true + 1")', 'silent write',
            'MetaMLCheck', 'call assert_equal(1, len(getqflist()))',
            "call assert_match('M03', getqflist()[0].text)", 'cclose',
        ]
    script += [
        'catch', 'call add(v:errors, v:exception . " at " . v:throwpoint)', 'endtry',
        'call writefile(v:errors, ' + quote(errors) + ')',
        'if !empty(v:errors) | cquit | endif', 'qa!',
    ]
    path = root / 'test.vim'
    path.write_text('\n'.join(script) + '\n')
    env = os.environ.copy()
    env.pop('VIMRUNTIME', None)
    env['HOME'] = str(root)
    args = [editor, '-u', 'NONE', '-i', 'NONE', '-n']
    args += ['--headless'] if 'nvim' in Path(editor).name else ['-N', '-e', '-s']
    result = subprocess.run(args + ['-S', str(path)], capture_output=True, text=True,
                            env=env, timeout=30)
    failures = errors.read_text() if errors.exists() else ''
    assert result.returncode == 0 and not failures, (editor, failures, result.stdout, result.stderr)
    calls = [json.loads(line) for line in log.read_text().splitlines()]
    assert calls[0][-1] == str(program)
    assert calls[0][0:2] == ['--stdlib', 'path with spaces']
    assert calls[1][-2:] == ['--', 'one argument; $literal']
    assert '--ast' in calls[2]
    assert calls[3] == ['-C', str(root), 'show']
    assert calls[4] == ['-C', str(root), 'check']
    assert calls[5] == ['-C', str(root), 'run', '--', 'project argument']
    print(Path(editor).name + ': detection, highlighting, indentation, commands passed')


def check_sublime():
    with (ADDONS / 'Sublime/Comments.tmPreferences').open('rb') as stream:
        preferences = plistlib.load(stream)
    assert preferences['scope'] == 'source.metaml'
    json.loads((ADDONS / 'Sublime/MetaML.sublime-settings').read_text())
    try:
        import yaml
    except ImportError:
        print('Sublime YAML/regex validation skipped (PyYAML unavailable)')
        return
    syntax = yaml.safe_load((ADDONS / 'Sublime/MetaML.sublime-syntax').read_text())
    assert set(syntax['file_extensions']) == {'mml', 'mmli', 'mlli', 'MAMMAL'}
    assert syntax['scope'] == 'source.metaml'
    for rules in syntax['contexts'].values():
        for rule in rules:
            for key in ['include', 'push']:
                if key in rule:
                    assert rule[key] in syntax['contexts']
            if 'match' in rule:
                pattern = re.sub(r'\{\{(\w+)\}\}',
                                 lambda m: syntax['variables'][m[1]], rule['match'])
                re.compile(pattern)
    print('Sublime: JSON, plist, YAML contexts and regexes validated')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--metaml', type=Path)
    options = parser.parse_args()
    scratch = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
    with tempfile.TemporaryDirectory(prefix='metaml-addons-', dir=scratch) as temporary:
        root = Path(temporary)
        for editor in ['vim', 'nvim']:
            executable = shutil.which(editor)
            if executable:
                check_editor(executable, root / editor,
                             options.metaml.resolve() if options.metaml else None)
        source = root / 'plugin source'
        shutil.copytree(ADDONS / 'Vim', source)
        destination = root / 'package with spaces'
        args = ['bash', str(source / 'install.sh'), 'nvim', str(destination)]
        subprocess.run(args, check=True, capture_output=True)
        assert destination.is_symlink() and destination.resolve() == source
        assert subprocess.run(args, capture_output=True).returncode == 1
        assert destination.resolve() == source
        print('Installer: paths with spaces and overwrite refusal passed')
    check_sublime()


if __name__ == '__main__':
    main()
