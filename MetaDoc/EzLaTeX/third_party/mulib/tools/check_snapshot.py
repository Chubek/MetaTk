#!/usr/bin/env python3
"""Verify dsymspec snapshot provenance using only the Python standard library.

This checks the embedded YAML/schema digests and inventory. Full deterministic
regeneration is a separate developer check: tools/dsymspec_generate.py --check.
"""
import hashlib
import json
from pathlib import Path
import re
import sys


def verify(root):
    snapshot = (root / 'include/mulib-dsymspec-data.inc').read_text()
    schema = (root / 'dsymspec/schema/dsymspec.schema.json').read_bytes()
    schema_hash = hashlib.sha256(schema).hexdigest()
    expected = {'dsymspec/' + p.name: p for p in
                (root / 'dsymspec').glob('*.dsymspec.yaml')}
    arrays = re.findall(
        r'^static const char \*const mulib_dsymspec_chunks_\d+_\[\] = \{\n(.*?)^\};',
        snapshot, re.M | re.S)
    seen = set()
    if not expected or len(arrays) != len(expected):
        raise ValueError('snapshot architecture count does not match dsymspec/')
    for array in arrays:
        chunks = []
        # A formatter may split one C array element into adjacent literals.
        # Decode each literal, but preserve/check commas and the NULL sentinel.
        pending = []
        terminated = False
        for line in array.splitlines():
            text = line.strip()
            if not text:
                continue
            if text == 'NULL':
                if pending or terminated:
                    raise ValueError('invalid snapshot sentinel')
                terminated = True
                continue
            if terminated:
                raise ValueError('data after snapshot sentinel')
            complete = text.endswith(',')
            literal = text[:-1] if complete else text
            chunk = json.loads(literal)
            if not isinstance(chunk, str):
                raise ValueError('invalid snapshot chunk')
            pending.append(chunk)
            if complete:
                chunks.append(''.join(pending))
                pending = []
        if pending or not terminated:
            raise ValueError('unterminated snapshot array')
        document = json.loads(''.join(chunks))
        provenance = document['_mulib_codegen']
        source = provenance['source']
        if source not in expected or source in seen:
            raise ValueError('unknown or duplicated snapshot source: ' + source)
        seen.add(source)
        digest = hashlib.sha256(expected[source].read_bytes()).hexdigest()
        if provenance['source_sha256'] != digest:
            raise ValueError('stale architecture snapshot: ' + source)
        if provenance['schema_sha256'] != schema_hash:
            raise ValueError('stale schema snapshot: ' + source)
    if seen != set(expected):
        raise ValueError('snapshot omits architecture sources')
    return len(seen)


def main():
    root = Path(__file__).resolve().parent.parent
    try:
        count = verify(root)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print('dsymspec snapshot: ' + str(error), file=sys.stderr)
        return 1
    print('dsymspec snapshot: %d architectures, source/schema digests match' % count)
    return 0


if __name__ == '__main__':
    sys.exit(main())
