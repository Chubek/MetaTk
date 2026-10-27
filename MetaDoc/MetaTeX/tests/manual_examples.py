"""Validate the 44-chapter manual and execute its explicitly marked examples."""
import html
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

executable = Path(sys.argv[1]).resolve()
manual = Path(sys.argv[2]).resolve()
chapters = sorted(manual.glob("[0-9][0-9]-*.md"))
assert len(chapters) == 44, f"expected 44 chapters, found {len(chapters)}"
index = (manual / "README.md").read_text()
for number, chapter in enumerate(chapters, 1):
    assert chapter.name.startswith(f"{number:02d}-"), chapter
    assert f"]({chapter.name})" in index, f"unindexed chapter: {chapter}"
    assert chapter.read_text().startswith(f"# {number:02d} — "), chapter

example_pattern = re.compile(r"<!-- metatex-example (\{[^\n]+\}) -->\s*```tex\n(.*?)\n```", re.S)
examples = []
for chapter in [manual / "README.md", *chapters]:
    content = chapter.read_text()
    # Ignore code fences when checking Markdown document links.
    prose = re.sub(r"```[^\n]*\n.*?\n```", "", content, flags=re.S)
    for link in re.findall(r"\]\(([^)]+)\)", prose):
        if "://" not in link and not link.startswith("#"):
            assert (chapter.parent / link.split("#")[0]).exists(), (chapter, link)
    found = list(example_pattern.finditer(content))
    assert len(found) == content.count("<!-- metatex-example "), f"malformed example in {chapter}"
    for match in found:
        metadata = json.loads(match[1])
        assert metadata.get("format") in ("plain", "latex", "nekotex"), chapter
        assert "text" in metadata or "contains" in metadata, chapter
        examples.append((chapter.name, metadata, match[2]))
assert examples, "manual has no executable examples"

with tempfile.TemporaryDirectory(prefix="metatex-manual-") as temporary:
    root = Path(temporary)
    for number, (chapter, metadata, source) in enumerate(examples):
        path = root / f"example-{number}.tex"
        path.write_text(source, encoding="utf-8")
        artifacts = {}
        for target in ("text", "json", "html", "pdf"):
            output = root / f"example-{number}.{target}"
            command = [str(executable), "-f", metadata["format"], "-T", target, "-o", str(output)]
            for plugin in metadata.get("plugins", []):
                command.extend(["--plugin", plugin])
            result = subprocess.run(command + [str(path)], capture_output=True, text=True, timeout=40)
            assert result.returncode == 0, (chapter, number, target, result.stdout, result.stderr)
            assert not result.stderr.strip(), (chapter, number, "unexpected diagnostic", result.stderr)
            artifacts[target] = output.read_bytes()
        actual = artifacts["text"].decode("utf-8").rstrip("\n")
        if "text" in metadata:
            assert actual == metadata["text"], (chapter, number, actual, metadata["text"])
        if "contains" in metadata:
            assert metadata["contains"] in actual, (chapter, number, actual, metadata["contains"])
        document = json.loads(artifacts["json"])
        assert document["format"] == "metatex-document-v1" and document["blocks"], (chapter, number)
        assert not document["diagnostics"], (chapter, document["diagnostics"])
        markup = artifacts["html"].decode("utf-8")
        assert "<html" in markup and "</html>" in markup, (chapter, number)
        # Every ordinary span's escaped text must survive HTML serialization.
        for block in document["blocks"]:
            for span in block["spans"]:
                if span["type"] in ("text", "bold", "italic", "code", "smallcaps", "underline") and span["text"]:
                    assert html.escape(span["text"], quote=False) in markup, (chapter, span)
        assert artifacts["pdf"].startswith(b"%PDF-") and b"%%EOF" in artifacts["pdf"], (chapter, number)
print(f"44 chapters and {len(examples)} examples verified in four output formats")
