#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -z "${JTML_BIN:-}" ]]; then
  "$repo_root/scripts/build_site_compiler.sh"
  revision="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["revision"])' "$repo_root/site/compiler.json")"
  JTML_BIN="$repo_root/build/site-compiler/$revision/build/jtml"
fi
python3 "$repo_root/scripts/build_public_site.py" \
  --output "${1:-"$repo_root/dist/site"}" \
  --compiler "$JTML_BIN" \
  --base-url "${SITE_BASE_URL:-https://mrjohnnyrocha.github.io/jtml/}"
