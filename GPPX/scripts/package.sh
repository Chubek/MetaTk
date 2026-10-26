#!/usr/bin/env sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
output="${1:-$root/GPPX-complete.zip}"
python3 - "$root" "$output" <<'PY'
import os, pathlib, sys, zipfile
root=pathlib.Path(sys.argv[1]).resolve()
output=pathlib.Path(sys.argv[2]).resolve()
skip={'build','build-release','build-asan','build-core','.git','__pycache__','.cache'}
with zipfile.ZipFile(output,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for file in sorted(root.rglob('*')):
        rel=file.relative_to(root)
        if any(part in skip for part in rel.parts) or not file.is_file() or file.resolve()==output:
            continue
        arcname=str(pathlib.PurePosixPath('GPPX',*rel.parts))
        info=zipfile.ZipInfo.from_file(file,arcname=arcname)
        info.compress_type=zipfile.ZIP_DEFLATED
        info.external_attr=(0o755 if os.access(file,os.X_OK) else 0o644)<<16
        z.writestr(info,file.read_bytes(),compress_type=zipfile.ZIP_DEFLATED,compresslevel=9)
print(output)
PY
