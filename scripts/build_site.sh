#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out_dir="${1:-"$repo_root/dist/site"}"

rm -rf "$out_dir"
mkdir -p "$out_dir"

cp -R "$repo_root/site/." "$out_dir/"
cp "$repo_root/docs/language-reference.md" "$out_dir/language-reference.md"
cp "$repo_root/docs/deployment.md" "$out_dir/deployment.md"
cp "$repo_root/README.md" "$out_dir/README.md"
cp "$repo_root/ROADMAP.md" "$out_dir/ROADMAP.md"

cat > "$out_dir/robots.txt" <<'ROBOTS'
User-agent: *
Allow: /

Sitemap: https://jtml.org/sitemap.xml
ROBOTS

cat > "$out_dir/sitemap.xml" <<'SITEMAP'
<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">
  <url><loc>https://jtml.org/</loc></url>
  <url><loc>https://jtml.org/tools.html</loc></url>
  <url><loc>https://jtml.org/examples.html</loc></url>
  <url><loc>https://jtml.org/reference.html</loc></url>
  <url><loc>https://jtml.org/security.html</loc></url>
  <url><loc>https://jtml.org/deploy.html</loc></url>
</urlset>
SITEMAP

find "$out_dir" -type f | sort > "$out_dir/manifest.txt"

echo "Built site artifact: $out_dir"
