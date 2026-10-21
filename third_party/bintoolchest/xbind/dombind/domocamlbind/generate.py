"""Generate DomOCamlBind's C header; accepts OUTPUT [--module NAME] [--prefix PREFIX]."""
import runpy
import sys
from pathlib import Path

if __name__ == "__main__":
    sys.argv.insert(1, "ocaml")
    runpy.run_path(str(Path(__file__).resolve().parent.parent / "generate.py"), run_name="__main__")
