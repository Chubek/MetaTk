"""Normalize accidental TeX escaping in verbatim source and detokenized code.

Safe to rerun after importing a chapter. Ordinary prose and texttt commands
are left alone; only lstlisting bodies and code/api arguments are normalized.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

def unescape(text):
    return text.replace(r'\_', '_').replace(r'\&', '&')

def normalize(text):
    text = re.sub(r'(\\begin\{lstlisting\}\[.*?\]\s*\n)(.*?)(\\end\{lstlisting\})',
                  lambda m: m[1] + unescape(m[2]) + m[3], text, flags=re.S)
    text = re.sub(r'(\\(?:code|api)\{)([^{}]*)(\})',
                  lambda m: m[1] + unescape(m[2]) + m[3], text)
    def table(match):
        body = match[0]
        result, depth, i = [], 0, 0
        while i < len(body):
            if body[i] == '\\' and i + 1 < len(body):
                pair = body[i:i+2]
                result.append('&' if pair == r'\&' and depth == 0 else pair)
                i += 2
                continue
            if body[i] == '{': depth += 1
            if body[i] == '}': depth -= 1
            result.append(body[i])
            i += 1
        return ''.join(result)
    return re.sub(r'\\begin\{tabular\}.*?\\end\{tabular\}', table, text, flags=re.S)

if __name__ == '__main__':
    for path in sorted((ROOT / 'chapters').glob('ch*.ltx')):
        before = path.read_text()
        after = normalize(before)
        if after != before:
            path.write_text(after)
            print('Normalized', path.name)
