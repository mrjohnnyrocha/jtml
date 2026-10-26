#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-"$repo_root/build"}"
out_dir="${1:-"$repo_root/dist/release"}"
build_type="${BUILD_TYPE:-Release}"

platform_args=(-DCMAKE_BUILD_TYPE="$build_type")
if [[ "$(uname -s)" == Darwin ]]; then
  # Explicit cache input overrides stale host/SDK defaults before project().
  platform_args+=(-DCMAKE_OSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-14.0}")
fi
cmake -S "$repo_root" -B "$build_dir" "${platform_args[@]}" \
  -DJTML_BUILD_PYTHON=OFF \
  -DJTML_BUILD_TESTS=ON \
  -DJTML_VERSION_SUFFIX="${JTML_VERSION_SUFFIX-dev}"
cmake --build "$build_dir" --target jtml_cli --parallel
cmake --build "$build_dir" --target jtml_c --parallel

rm -rf "$out_dir"
mkdir -p "$out_dir/bin" "$out_dir/lib" "$out_dir/include/jtml" "$out_dir/docs" "$out_dir/examples" "$out_dir/tutorial" "$out_dir/studio" "$out_dir/editors" "$out_dir/site"

cp "$build_dir/jtml" "$out_dir/bin/jtml"
find "$build_dir" -maxdepth 1 \( -name 'libjtml.*' -o -name 'jtml.dll' \) -type f -exec cp {} "$out_dir/lib/" \;
cp "$repo_root/include/jtml/c_api.h" "$out_dir/include/jtml/c_api.h"
cp "$repo_root/README.md" "$repo_root/ROADMAP.md" "$repo_root/LICENSE" "$repo_root/THIRD_PARTY_NOTICES.md" "$out_dir/"
mkdir -p "$out_dir/third_party"
cp -R "$repo_root/third_party/licenses" "$out_dir/third_party/"
cp -R "$repo_root/docs/." "$out_dir/docs/"
cp -R "$repo_root/examples/." "$out_dir/examples/"
cp -R "$repo_root/tutorial/." "$out_dir/tutorial/"
cp -R "$repo_root/studio/." "$out_dir/studio/"
tar -C "$repo_root/editors" --exclude=node_modules -cf - . | tar -C "$out_dir/editors" -xf -
cp -R "$repo_root/site/." "$out_dir/site/"

cat > "$out_dir/INSTALL.md" <<'INSTALL'
# JTML Release Artifact

Run:

```sh
./bin/jtml --version
./bin/jtml examples
./bin/jtml demo --port 8000
./bin/jtml tutorial --port 8000
./bin/jtml studio --port 8000
./bin/jtml serve examples/friendly_counter.jtml --port 8000
./bin/jtml build examples/friendly_import_page.jtml --out dist
```

Compiler inputs above are relative to the archive root. The executable finds its
bundled examples/tutorial/Studio/docs independently of the working directory; use
absolute source paths when invoking check/build from another directory.
`doctor` audits a source checkout,
not a standalone installation. ARTIFACT.json records version, platform, architecture
and source revision/dirty status. Development artifacts are not a published release.

macOS arm64 packages target macOS 14.0. MACOS_COMPATIBILITY.json records the CLI
and C API Mach-O audit; hosted macOS 15 success alone does not prove macOS 14
runtime compatibility. Use only a regenerated artifact whose exact SHA/checksums
also have a successful macOS 14 runtime report. The macOS artifact from workflow
40150110121 requires macOS 15 and must not be used on macOS 14.

JTML-owned material: Apache-2.0. Read LICENSE and THIRD_PARTY_NOTICES.md;
third-party components retain their own licenses under third_party/licenses/.
The editor is source-only here; no node_modules or published VSIX is included.

This archive includes the CLI, the C ABI shared library + header, examples,
tutorial lessons, Studio samples/reference/sidebar catalogs, docs, editor support,
the predeploy static site source, a file manifest, and SHA-256 checksums.
INSTALL

if [[ "$(uname -s)" == Darwin ]]; then
  python3 "$repo_root/scripts/check_macos_distribution.py" "$out_dir" \
    --output "$out_dir/MACOS_COMPATIBILITY.json"
fi

version="$(cat "$build_dir/jtml-version.txt")"
test "$("$out_dir/bin/jtml" --version)" = "jtml $version"
platform="$(uname -s | tr '[:upper:]' '[:lower:]')"
architecture="$(uname -m)"
case "$platform" in
  darwin|linux) ;;
  *) echo "error: binary archive platform not validated: $platform" >&2; exit 1 ;;
esac
archive_base="$repo_root/dist/jtml-$version-$platform-$architecture"
mkdir -p "$(dirname "$archive_base")"
revision="$(git -C "$repo_root" rev-parse HEAD)"
dirty=false
if [[ -n "$(git -C "$repo_root" status --porcelain --untracked-files=normal)" ]]; then dirty=true; fi
epoch="${SOURCE_DATE_EPOCH:-$(git -C "$repo_root" show -s --format=%ct HEAD)}"
python3 "$repo_root/scripts/create_release_archive.py" "$out_dir" "$archive_base.tar.gz" \
  --version "$version" --platform "$platform" --architecture "$architecture" \
  --revision "$revision" --dirty "$dirty" --epoch "$epoch"

echo "Built CLI release directory: $out_dir"
echo "Built archive: $archive_base.tar.gz"
