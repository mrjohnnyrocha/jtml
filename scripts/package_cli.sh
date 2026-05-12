#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-"$repo_root/build"}"
out_dir="${1:-"$repo_root/dist/release"}"
build_type="${BUILD_TYPE:-Release}"

cmake -S "$repo_root" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE="$build_type" \
  -DJTML_BUILD_PYTHON=OFF \
  -DJTML_BUILD_TESTS=ON
cmake --build "$build_dir" --target jtml_cli --parallel
cmake --build "$build_dir" --target jtml_c --parallel

rm -rf "$out_dir"
mkdir -p "$out_dir/bin" "$out_dir/lib" "$out_dir/include/jtml" "$out_dir/docs" "$out_dir/examples" "$out_dir/tutorial" "$out_dir/editors" "$out_dir/site"

cp "$build_dir/jtml" "$out_dir/bin/jtml"
find "$build_dir" -maxdepth 1 \( -name 'libjtml.*' -o -name 'jtml.dll' \) -type f -exec cp {} "$out_dir/lib/" \;
cp "$repo_root/include/jtml/c_api.h" "$out_dir/include/jtml/c_api.h"
cp "$repo_root/README.md" "$repo_root/ROADMAP.md" "$out_dir/"
cp -R "$repo_root/docs/." "$out_dir/docs/"
cp -R "$repo_root/examples/." "$out_dir/examples/"
cp -R "$repo_root/tutorial/." "$out_dir/tutorial/"
cp -R "$repo_root/editors/." "$out_dir/editors/"
cp -R "$repo_root/site/." "$out_dir/site/"

cat > "$out_dir/INSTALL.md" <<'INSTALL'
# JTML Release Artifact

Run:

```sh
./bin/jtml --version
./bin/jtml examples
./bin/jtml doctor
./bin/jtml demo --port 8000
./bin/jtml tutorial --port 8000
./bin/jtml serve examples/friendly_counter.jtml --port 8000
./bin/jtml build examples/friendly_import_page.jtml --out dist
```

This archive includes the CLI, the C ABI shared library + header, examples,
tutorial lessons, docs, editor support, the predeploy static site source, a
file manifest, and SHA-256 checksums.
INSTALL

find "$out_dir" -type f | sort > "$out_dir/MANIFEST.txt"
(
  cd "$out_dir"
  LC_ALL=C LANG=C find . -type f ! -name SHA256SUMS -print0 \
    | sort -z \
    | LC_ALL=C LANG=C xargs -0 shasum -a 256 > SHA256SUMS
)

archive_base="$repo_root/dist/jtml-$(uname -s | tr '[:upper:]' '[:lower:]')-$(uname -m)"
rm -f "$archive_base.tar.gz"
LC_ALL=C LANG=C tar -C "$(dirname "$out_dir")" -czf "$archive_base.tar.gz" "$(basename "$out_dir")"

echo "Built CLI release directory: $out_dir"
echo "Built archive: $archive_base.tar.gz"
