#!/usr/bin/env python3
"""Validate expanded gppx-vmwv v1 catalogues using only the Python stdlib.

This checks structure and symbolic operand arity, not machine execution.
"""
import argparse
import copy
import json
from pathlib import Path
import re
import sys


class InvalidSpec(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise InvalidSpec(message)


def nonempty_text(value):
    return isinstance(value, str) and bool(value.strip())


def validate(spec):
    require(isinstance(spec, dict), 'root must be an object')
    fields = {'format', 'version', 'id', 'name', 'revision', 'scope', 'model',
              'state', 'invariants', 'operations', 'example', 'references', 'notes'}
    require(set(spec) == fields, 'unexpected or missing top-level fields')
    require(spec['format'] == 'gppx-vmwv', 'unknown format')
    require(type(spec['version']) is int and spec['version'] == 1, 'unsupported version')
    require(nonempty_text(spec['id']) and re.fullmatch(r'[a-z0-9]+(?:-[a-z0-9]+)*', spec['id']),
            'id must be a lower-case hyphen-separated identifier')
    for key in ('name', 'revision', 'notes'):
        require(nonempty_text(spec[key]), key + ' must be nonempty text')
    require(spec['scope'] == 'reference-subset', 'unknown scope')
    require(spec['model'] in ('stack', 'register', 'transition', 'closure', 'graph',
                              'logic', 'tape', 'comparison'), 'unknown model')
    require(isinstance(spec['state'], dict) and spec['state'], 'state must be a nonempty object')
    for name, description in spec['state'].items():
        require(nonempty_text(name) and nonempty_text(description), 'invalid state description')
    for key in ('invariants', 'references'):
        require(isinstance(spec[key], list) and spec[key], key + ' must be a nonempty array')
        require(all(nonempty_text(x) for x in spec[key]), key + ' entries must be nonempty text')
    require(isinstance(spec['operations'], list) and spec['operations'], 'operations must be a nonempty array')
    operations = {}
    for op in spec['operations']:
        require(isinstance(op, dict) and set(op) == {'name', 'operands', 'effect'}, 'invalid operation fields')
        require(nonempty_text(op['name']) and nonempty_text(op['effect']), 'invalid operation name/effect')
        require(op['name'] not in operations, 'duplicate operation: ' + op['name'])
        require(isinstance(op['operands'], list), 'operands must be an array')
        require(all(nonempty_text(x) for x in op['operands']), 'invalid operand name')
        require(len(set(op['operands'])) == len(op['operands']), 'duplicate operand name')
        operations[op['name']] = len(op['operands'])
    example = spec['example']
    require(isinstance(example, dict) and set(example) == {'initial', 'steps', 'expected'}, 'invalid example fields')
    require(nonempty_text(example['initial']) and nonempty_text(example['expected']), 'missing example pre/postcondition')
    require(isinstance(example['steps'], list) and example['steps'], 'example must contain steps')
    for step in example['steps']:
        require(isinstance(step, dict) and set(step) == {'op', 'args'}, 'invalid example step')
        require(nonempty_text(step['op']) and step['op'] in operations, 'example uses unknown operation')
        require(isinstance(step['args'], list) and len(step['args']) == operations[step['op']],
                'wrong operand count for ' + step['op'])
        require(all(type(x) in (str, int, bool) or x is None for x in step['args']), 'invalid symbolic operand')
    return spec['id']


def reject_duplicate_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise InvalidSpec('duplicate JSON key: ' + key)
        result[key] = value
    return result


def self_test(valid):
    mutations = [
        lambda s: s.update(version=2),
        lambda s: s.update(version=True),
        lambda s: s.update(state={}),
        lambda s: s['operations'].append(copy.deepcopy(s['operations'][0])),
        lambda s: s['example']['steps'][0].update(op='unknown-operation'),
        lambda s: s['example']['steps'][0]['args'].append('extra-operand'),
        lambda s: s.update(references=[]),
    ]
    for mutate in mutations:
        spec = copy.deepcopy(valid)
        mutate(spec)
        try:
            validate(spec)
        except InvalidSpec:
            continue
        raise InvalidSpec('validator self-test accepted an invalid document')
    try:
        json.loads('{"x":1,"x":2}', object_pairs_hook=reject_duplicate_keys)
    except InvalidSpec:
        return
    raise InvalidSpec('validator accepted duplicate JSON keys')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('files', nargs='+', type=Path)
    args = parser.parse_args()
    seen = set()
    try:
        for path in args.files:
            spec = json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=reject_duplicate_keys)
            identifier = validate(spec)
            require(identifier not in seen, 'duplicate catalogue id: ' + identifier)
            require(identifier == path.stem.replace(' ', '-').lower(), 'id does not match filename')
            seen.add(identifier)
            if args.self_test:
                self_test(spec)
    except (OSError, ValueError, TypeError) as error:
        print(f'{path}: {error}', file=sys.stderr)
        return 1
    print(f'Validated {len(seen)} VM specifications (structure and operand arity).')
    return 0


if __name__ == '__main__':
    sys.exit(main())
