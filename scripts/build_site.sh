#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
python3 "$repo_root/scripts/build_public_site.py" \
  --output "${1:-"$repo_root/dist/site"}" \
  --compiler "${JTML_BIN:-"$repo_root/build/jtml"}" \
  --base-url "${SITE_BASE_URL:-https://mrjohnnyrocha.github.io/jtml/}"
