import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / ".." / "third_party" / "breathe"))
sys.path.insert(0, str(ROOT / ".." / "third_party" / "exhale"))

project = "EkippX"
author = "EkippX"
extensions = ["breathe", "exhale"]
templates_path = ["_templates"]
exclude_patterns = []
html_static_path = ["_static"]
master_doc = "index"

breathe_projects = {"EkippX": str(ROOT / "_build" / "doxygen" / "xml")}
breathe_default_project = "EkippX"

exhale_args = {
    "containmentFolder": "./api",
    "rootFileName": "library_root.rst",
    "rootFileTitle": "EkippX API",
    "doxygenStripFromPath": str(ROOT.parent),
    "createTreeView": True,
}
