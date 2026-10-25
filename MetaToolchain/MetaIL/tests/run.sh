#!/bin/sh
set -eu
if [ "$#" -ne 1 ]; then echo 'Usage: tests/run.sh <path to metail executable>' >&2; exit 2; fi
METAIL_HOME="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)" export METAIL_HOME
python3 "$METAIL_HOME/tests/test_metail.py" "$1"
