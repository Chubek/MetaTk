#!/usr/bin/env python3
"""Interface pairing and end-to-end MAMMAL project regression tests."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--metaml', required=True, type=Path)
    parser.add_argument('--legacy', type=Path)
    parser.add_argument('--mammal', required=True, type=Path)
    options = parser.parse_args()
    metaml, mammal = options.metaml.resolve(), options.mammal.resolve()

    with tempfile.TemporaryDirectory(prefix='metaml-mammal-') as temporary:
        root = Path(temporary)

        def write(name, text):
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)

        def call(binary, *args, ok=True):
            result = subprocess.run([str(binary), *map(str, args)], cwd=root,
                                    text=True, capture_output=True)
            assert (result.returncode == 0) == ok, (args, result.returncode, result.stdout, result.stderr)
            return result

        write('foo.mmli', 'type Answer := Int\nanswer :: Answer\n')
        write('foo.mml', 'answer := 42\nanswer\n')
        assert call(metaml, '--no-stdlib', 'foo.mml').stdout.strip() == '42'
        call(metaml, '--no-stdlib', '--check', 'foo.mml')
        call(metaml, '--no-stdlib', 'foo.mmli')
        if options.legacy:
            call(options.legacy.resolve(), '--no-stdlib', 'foo.mml')
        write('foo.mml', 'answer := "wrong"\nIO.println("MUST NOT RUN")\n')
        result = call(metaml, 'foo.mml', ok=False)
        assert 'MUST NOT RUN' not in result.stdout
        call(metaml, '--check', 'foo.mml', ok=False)
        if options.legacy:
            call(options.legacy.resolve(), 'foo.mml', ok=False)
        write('foo.mml', 'other := 42\n')
        assert 'required by the signature' in call(metaml, 'foo.mml', ok=False).stderr
        # Ambient bindings cannot satisfy a missing implementation member.
        call(metaml, '-l', 'foo.mmli', 'foo.mml', ok=False)
        write('foo.mml', 'answer := 42\n')
        write('foo.mlli', 'answer :: Int\n')
        call(metaml, 'foo.mml', ok=False)
        (root / 'foo.mmli').unlink()
        call(metaml, 'foo.mml')
        for bad in ['x := 1\n', 'IO.println("bad")\n', 'import "foo"\n',
                    'module Bad := struct val x := 1 end\n', '%quit\n']:
            write('bad.mmli', bad)
            call(metaml, 'bad.mmli', ok=False)
            call(metaml, '--check', 'bad.mmli', ok=False)
            if options.legacy:
                call(options.legacy.resolve(), '--no-stdlib', 'bad.mmli', ok=False)
        write('uses.mml', 'import "foo"\nanswer\n')
        call(metaml, 'uses.mml')
        write('foo.mlli', 'answer :: String\n')
        call(metaml, 'uses.mml', ok=False)
        call(metaml, '--check', 'uses.mml', ok=False)

        def contract(interface, implementation, ok):
            write('contract.mmli', interface)
            write('contract.mml', implementation)
            call(metaml, 'contract.mml', ok=ok)
            call(metaml, '--check', 'contract.mml', ok=ok)
            if options.legacy:
                call(options.legacy.resolve(), '--no-stdlib', 'contract.mml', ok=ok)

        contract('type Token\nmake :: Int -> Token\n',
                 'type Token := Int\nmake x := x\n', True)
        contract('type Token\n', 'other := 1\n', False)
        contract('type Number := Int\n', 'type Number := String\n', False)
        contract("identity :: 'a -> 'a\n", 'identity x := 1\n', False)
        contract("identity :: 'a -> 'a\n", 'identity x := x\n', True)
        contract("type Box 'a := | Box 'a\nbox :: 'a -> Box 'a\n",
                 'box x := Box x\n', True)
        contract('type Box := | Box Int\n', 'type Box := | Box String\n', False)
        contract('type Box := | Box Int\n', 'type Box := | Box Int Int\n', False)

        write('src/main.mmli', 'answer :: Int\n')
        write('src/main.mml', '''answer := 42
if Process.args(()) == ["argument with spaces"]
then IO.println("project works") else Core.error("program arguments lost")
''')
        write('tests/pass.mml', 'if true then () else Core.error("failed")\n')
        write('fixtures/input.txt', 'fixture contents\n')
        write('assets/banner.txt', 'artifact contents\n')
        write('MAMMAL', '''project_name := "example" ^ "-computed"
Mammal.project(project_name)
Mammal.directory("src")
Mammal.module_path("src")
Mammal.main("src/main.mml")
Mammal.test("tests/pass.mml")
Mammal.fixture("fixtures/input.txt", "input.txt")
Mammal.artifact("assets/banner.txt", "share/banner.txt")
Mammal.install("share/banner.txt", "share/example/banner.txt")
''')
        result = call(mammal, 'show')
        assert 'example-computed' in result.stdout and 'src/main.mmli' in result.stdout
        call(mammal, '-C', root / 'src', 'check')
        assert not (root / '_mammal').exists()
        call(mammal, 'build')
        assert (root / '_mammal/src/main.mmli').exists()
        assert (root / '_mammal/input.txt').read_text() == 'fixture contents\n'
        assert 'project works' in call(mammal, 'run', '--', 'argument with spaces').stdout
        call(mammal, 'test')
        write('tests/pass.mml', 'Core.error("test failure")\n')
        call(mammal, 'test', ok=False)
        write('tests/pass.mml', '()\n')
        call(mammal, '--prefix', '/usr', '--destdir', root / 'stage', 'install')
        assert (root / 'stage/usr/share/example/banner.txt').read_text() == 'artifact contents\n'
        original = (root / 'MAMMAL').read_text()
        write('MAMMAL', original + 'Mammal.fixture("assets/banner.txt", "../escape")\n')
        call(mammal, 'build', ok=False)
        assert not (root / 'escape').exists()
        write('MAMMAL', original + 'Mammal.fixture("assets/banner.txt", "input.txt")\n')
        call(mammal, 'build', ok=False)
        write('MAMMAL', original + 'Mammal.install("share/banner.txt", "../escape")\n')
        call(mammal, '--prefix', root / 'prefix', 'install', ok=False)
        # Symlink escapes must also be rejected.
        (root / 'outside').symlink_to(root.parent, target_is_directory=True)
        write('MAMMAL', original + 'Mammal.fixture("assets/banner.txt", "outside/escape")\n')
        (root / '_mammal/outside').symlink_to(root.parent, target_is_directory=True)
        call(mammal, 'build', ok=False)
        write('MAMMAL', 'Mammal.project(123)\n')
        call(mammal, 'show', ok=False)
        call(mammal, 'unknown', ok=False)
        (root / 'MAMMAL').unlink()
        call(mammal, 'init')
        call(mammal, 'init', ok=False)
    print('interface and MAMMAL checks passed')


if __name__ == '__main__':
    main()
