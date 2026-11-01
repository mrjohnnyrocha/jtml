#!/usr/bin/env bash
# Build an immutable compiler source snapshot, independently of website edits.
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
revision="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["revision"])' "$repo_root/site/compiler.json")"
compiler_root="$repo_root/build/site-compiler/$revision"
if ! git -C "$repo_root" cat-file -e "$revision^{commit}"; then
  git -C "$repo_root" fetch origin "$revision"
fi
mkdir -p "$compiler_root/source"
# Always restore the exact tracked snapshot; no source from the working tree.
git -C "$repo_root" archive "$revision" | tar -x -C "$compiler_root/source"
cmake -S "$compiler_root/source" -B "$compiler_root/build" \
  -DJTML_BUILD_TESTS=OFF -DJTML_BUILD_PYTHON=OFF -DJTML_VERSION_SUFFIX=dev
cmake --build "$compiler_root/build" --parallel "${JTML_BUILD_JOBS:-4}"
python3 - "$repo_root/site/compiler.json" "$compiler_root/build/jtml" <<'PROVENANCE'
import hashlib,json,pathlib,subprocess,sys
pin=json.loads(pathlib.Path(sys.argv[1]).read_text())
binary=pathlib.Path(sys.argv[2])
assert subprocess.check_output([binary,'--version'],text=True).strip()==pin['version']
pin['binarySHA256']=hashlib.sha256(binary.read_bytes()).hexdigest()
pin['origin']='git archive of pinned commit; CMake C++17; Python/tests off; version suffix dev'
pathlib.Path(str(binary)+'.provenance.json').write_text(json.dumps(pin,sort_keys=True,indent=2)+'\n')
PROVENANCE
