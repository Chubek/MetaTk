"""Generate any DomBind aggregate or individual Q module.

Requires the repository's q package (domlibs/domqlib), as does dompybind.
No import-path mutation or third-party downloads are performed.
"""
from __future__ import annotations

import argparse
from pathlib import Path

LANGUAGES = ('d', 'go', 'java', 'lua', 'node', 'ocaml', 'ruby', 'rust')
ROOT = Path(__file__).resolve().parent


def generate(language: str, output: Path, module: str | None = None,
             prefix: str | None = None) -> None:
    from q import QCompiler

    if language not in LANGUAGES:
        raise ValueError(f'Unsupported language: {language}')
    directory = ROOT / f'dom{language}bind'
    source = directory / f'{module or language + "bind"}.q'
    if source.parent.resolve() != directory.resolve():
        raise ValueError('Module must be a simple module name')
    compiler = QCompiler(search_path=[str(directory), str(ROOT.parent / 'domqlib')])
    header = compiler.compile(str(source), **({'PREFIX': prefix} if prefix else {}))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(header, encoding='utf-8')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('language', choices=LANGUAGES)
    parser.add_argument('output', type=Path)
    parser.add_argument('--module', help='One module instead of the aggregate')
    parser.add_argument('--prefix', help='Override the selected module prefix')
    args = parser.parse_args()
    generate(args.language, args.output, args.module, args.prefix)


if __name__ == '__main__':
    main()
