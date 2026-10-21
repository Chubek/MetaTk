#!/usr/bin/env python3
"""Validate the ArchBFD documents against ArchBFD/schema/MetaBFD.schema.json.

The check is optional: it needs PyYAML and jsonschema.  When either is
missing the script explains what is missing and exits with 77, the exit
status CTest uses for "skipped".

    python3 tests/check_bfd_schema.py [ARCHBFD_DIR]
"""

import json
import os
import sys

try:
    import yaml
except ImportError:  # pragma: no cover - depends on the environment
    sys.stderr.write("check_bfd_schema: PyYAML is not installed\n")
    sys.exit(77)

try:
    import jsonschema
except ImportError:  # pragma: no cover - depends on the environment
    sys.stderr.write("check_bfd_schema: jsonschema is not installed\n")
    sys.exit(77)


def main(argv):
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    root = argv[0] if argv else os.path.join(here, "ArchBFD")
    schema_path = os.path.join(root, "schema", "MetaBFD.schema.json")
    with open(schema_path, "r", encoding="utf-8") as fh:
        schema = json.load(fh)

    validator = jsonschema.Draft202012Validator(schema)
    failures = 0
    checked = 0
    for name in sorted(os.listdir(root)):
        if not name.endswith(".bfd.yaml"):
            continue
        path = os.path.join(root, name)
        with open(path, "r", encoding="utf-8") as fh:
            document = yaml.safe_load(fh)
        errors = sorted(validator.iter_errors(document), key=lambda e: list(e.path))
        for error in errors:
            where = "/".join(str(p) for p in error.path)
            sys.stderr.write("%s: %s: %s\n" % (path, where or "/", error.message))
            failures += 1
        checked += 1
    print("%d document(s) checked, %d schema error(s)" % (checked, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
