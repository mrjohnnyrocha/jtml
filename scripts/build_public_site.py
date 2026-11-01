#!/usr/bin/env python3
"""Assemble the JTML homepage and real JTML demo; never deploy anything."""
import argparse
import hashlib
import html
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from urllib.parse import urlsplit
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parents[1]
CSP = "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'none'; object-src 'none'; base-uri 'self'; form-action 'none'; frame-src 'self'"
ASSETS = ('index.html', 'jtml-runtime.js', 'components/index.js', 'app.js')


def digest(file):
    return hashlib.sha256(file.read_bytes()).hexdigest()


def build(output, compiler, base_url):
    url = urlsplit(base_url)
    if url.scheme not in ('https', 'http') or not url.hostname or url.username or url.password or url.query or url.fragment:
        raise ValueError('SITE_BASE_URL must be an absolute HTTP(S) URL without credentials/query/fragment')
    base_url = base_url.rstrip('/') + '/'
    compiler = shutil.which(compiler) or str(Path(compiler).resolve())
    version = subprocess.check_output([compiler, '--version'], text=True).strip()
    output = output.resolve()
    # Never allow a caller to remove a source tree or its ancestors.
    if output == ROOT or ROOT.is_relative_to(output) or (output.is_relative_to(ROOT) and (not output.is_relative_to(ROOT / 'dist') or output == ROOT / 'dist')):
        raise ValueError('Output must not replace the repository or site source')
    compiler_info = dict(version=version, binarySHA256=digest(Path(compiler)), origin='caller-supplied binary; source revision unverified')
    sidecar = Path(compiler + '.provenance.json')
    if sidecar.exists():
        compiler_info = json.loads(sidecar.read_text())
        assert compiler_info['binarySHA256'] == digest(Path(compiler)), 'Compiler provenance/binary mismatch'
        assert compiler_info['version'] == version
    source = ROOT / 'examples/friendly_counter.jtml'
    with tempfile.TemporaryDirectory(prefix='jtml-site-counter-') as temporary:
        work = Path(temporary)
        shutil.copyfile(source, work / 'counter.jtml')
        subprocess.run([compiler, 'build', 'counter.jtml', '--target', 'browser', '--out', 'generated'], cwd=work, check=True)
        generated = work / 'generated'
        assert {p.relative_to(generated).as_posix() for p in generated.rglob('*') if p.is_file()} == set(ASSETS)
        for name in ASSETS:
            text = (generated / name).read_text()
            assert not re.search(r'\bnew\s+Function\b|\beval\s*\(', text), name
            if name == 'index.html':
                assert not re.search(r'\son\w+\s*=', text, re.I)
                assert not re.search(r'<script\b(?![^>]*\bsrc=)(?![^>]*\btype="application/json")', text, re.I)
        assert (generated / 'jtml-runtime.js').stat().st_size <= 273000
        original_hashes = {name: digest(generated / name) for name in ASSETS}
        if output.exists():
            shutil.rmtree(output)
        shutil.copytree(ROOT / 'site', output, ignore=shutil.ignore_patterns('CNAME'))
        # Compile the actual tracked homepage, including its component definitions.
        homepage = work / 'homepage'
        (work / 'homepage-source').mkdir()
        shutil.copyfile(ROOT / 'site/index.jtml', work / 'homepage-source/index.jtml')
        subprocess.run([compiler, 'build', 'homepage-source/index.jtml', '--target', 'browser', '--out', 'homepage'], cwd=work, check=True)
        assert {p.relative_to(homepage).as_posix() for p in homepage.rglob('*') if p.is_file()} == set(ASSETS)
        homepage_hashes = {name: digest(homepage / name) for name in ASSETS}
        for name in ASSETS:
            content = (homepage / name).read_text()
            assert not re.search(r'\bnew\s+Function\b|\beval\s*\(', content), name
            assert 'jtml-update-plans.js' not in content, name
        shutil.copytree(homepage, output, dirs_exist_ok=True)
        demo = output / 'demo/counter'
        shutil.copytree(generated, demo)
    shutil.copyfile(source, output / 'counter.jtml')
    shutil.copytree(ROOT / 'docs', output / 'docs')
    for name in ('README.md', 'ROADMAP.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md'):
        shutil.copyfile(ROOT / name, output / name)
    for name, original in [('language-reference.md', 'docs/reference/language-reference.md'), ('deployment.md', 'docs/tooling/deployment.md')]:
        shutil.copyfile(ROOT / original, output / name)
    shutil.copytree(ROOT / 'third_party/licenses', output / 'third_party/licenses')
    pages = sorted(output.glob('*.html')) + [demo / 'index.html']
    for page in pages:
        text = page.read_text()
        assert '<head>' in text
        additions = '<meta http-equiv="Content-Security-Policy" content="' + html.escape(CSP, quote=True) + '">\n'
        location = page.relative_to(output).as_posix()
        canonical = base_url + ('' if location == 'index.html' else location.replace('demo/counter/index.html', 'demo/counter/'))
        additions += '<link rel="canonical" href="' + html.escape(canonical, quote=True) + '">\n'
        text = text.replace('<head>', '<head>\n' + additions, 1)
        if page == output / 'index.html':
            # Packaging metadata only. Never rewrite compiled body or JavaScript.
            text = text.replace('<html>', '<html lang="en">', 1)
            text = re.sub(r'<title>.*?</title>', '<title>JTML — readable source, observable behavior</title>', text, count=1)
            text = text.replace('</head>', '<meta name="description" content="Friendly JTML 2: markup, state, actions and components in a readable UI language. Explore the experimental RC.3 release and a genuine compiled Counter.">\n<meta name="viewport" content="width=device-width, initial-scale=1">\n<link rel="stylesheet" href="./home.css">\n</head>', 1)
        page.write_text(text)
    for name in ASSETS[1:]:
        assert digest(demo / name) == original_hashes[name], 'Generated JavaScript must remain untouched'
    for name in ASSETS[1:]:
        assert digest(output / name) == homepage_hashes[name], 'Homepage JavaScript must remain untouched'
    locations = [base_url + ('' if p.name == 'index.html' else p.name) for p in sorted(output.glob('*.html'))]
    locations.append(base_url + 'demo/counter/')
    (output / 'sitemap.xml').write_text('<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n' + ''.join('  <url><loc>' + escape(u) + '</loc></url>\n' for u in locations) + '</urlset>\n')
    (output / 'robots.txt').write_text('User-agent: *\nAllow: /\n\nSitemap: ' + base_url + 'sitemap.xml\n')
    revision = subprocess.check_output(['git', '-C', ROOT, 'rev-parse', 'HEAD'], text=True).strip()
    dirty = bool(subprocess.check_output(['git', '-C', ROOT, 'status', '--porcelain', '--untracked-files=normal'], text=True))
    info = dict(schema=2, compiler=compiler_info, homepageSource='site/index.jtml', homepageSourceSHA256=digest(ROOT / 'site/index.jtml'), homepageGeneratedAssetsSHA256=homepage_hashes, sourceRevision=revision, sourceDirty=dirty, compilerVersion=version,
                baseURL=base_url, counterSource='examples/friendly_counter.jtml', counterSourceSHA256=digest(source),
                generatedAssetsSHA256=original_hashes, demoPolicy='Only document head/language metadata is added to compiled HTML; body and generated JavaScript are unchanged')
    (output / 'site-build.json').write_text(json.dumps(info, sort_keys=True, indent=2) + '\n')
    files = sorted(p for p in output.rglob('*') if p.is_file() and p != output / 'manifest.txt')
    (output / 'manifest.txt').write_text(''.join(digest(p) + '  ' + p.relative_to(output).as_posix() + '\n' for p in files))
    print('Built static site (no deployment):', output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--base-url', required=True)
    args = parser.parse_args()
    build(args.output, args.compiler, args.base_url)
