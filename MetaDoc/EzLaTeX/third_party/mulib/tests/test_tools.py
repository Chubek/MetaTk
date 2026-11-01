"""Regression checks for snapshot C literals and dependency compatibility groups."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parent.parent


def module(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'tools' / (name + '.py'))
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


snapshot = module('check_snapshot')
strata = module('stratify')


class Tools(unittest.TestCase):
    def test_snapshot(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'include').mkdir()
            (root / 'dsymspec/schema').mkdir(parents=True)
            source, schema = b'schema: 1\n', b'{}\n'
            (root / 'dsymspec/test.dsymspec.yaml').write_bytes(source)
            (root / 'dsymspec/schema/dsymspec.schema.json').write_bytes(schema)
            data = {'_mulib_codegen': {
                'source': 'dsymspec/test.dsymspec.yaml',
                'source_sha256': hashlib.sha256(source).hexdigest(),
                'schema_sha256': hashlib.sha256(schema).hexdigest()}}
            wire = json.dumps(data)
            # C concatenates adjacent literals before treating the comma as
            # the end of the array element; formatting must not break checks.
            literals = '\n'.join('  ' + json.dumps(wire[i:i+23])
                                 for i in range(0, len(wire), 23))
            text = ('static const char *const mulib_dsymspec_chunks_0_[] = {\n'
                    + literals + ',\n  NULL\n};\n')
            path = root / 'include/mulib-dsymspec-data.inc'
            path.write_text(text)
            self.assertEqual(snapshot.verify(root), 1)
            for invalid in (text.replace('  NULL\n', ''),
                            text.replace('  NULL\n', '  NULL\n  NULL\n'),
                            text.replace('  NULL\n', '  NULL\n  "tail",\n')):
                with self.subTest(invalid=invalid):
                    path.write_text(invalid)
                    with self.assertRaises(ValueError):
                        snapshot.verify(root)
            path.write_text(text)
            (root / 'dsymspec/test.dsymspec.yaml').write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError, 'stale architecture'):
                snapshot.verify(root)

    def test_strata(self):
        original = strata.ROOT
        try:
            with tempfile.TemporaryDirectory() as tmp:
                strata.ROOT = Path(tmp)
                directory = strata.ROOT / 'include'
                directory.mkdir()
                (directory / 'mulib-a.h').write_text('#include "mulib-b.h"\n')
                (directory / 'mulib-b.h').write_text('#include "mulib-a.h"\n')
                (directory / 'mulib-c.h').write_text('#include "mulib-a.h"\n')
                result = strata.render()
                self.assertIn('Compatibility group: `a`, `b`.', result)
                self.assertIn('Stratum 0 (2 modules)', result)
                self.assertIn('Stratum 1 (1 modules)', result)
                (directory / 'mulib-b.h').write_text('#include "mulib-missing.h"\n')
                with self.assertRaisesRegex(ValueError, 'unknown module'):
                    strata.render()
        finally:
            strata.ROOT = original


if __name__ == '__main__':
    unittest.main()
