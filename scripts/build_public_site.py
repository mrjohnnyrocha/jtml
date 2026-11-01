#!/usr/bin/env python3
"""Compile the JTML public pages and original Counter; never deploy anything."""
import argparse
import gzip
import hashlib
import html
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zlib
from urllib.parse import urlsplit
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parents[1]
CSP = "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'none'; object-src 'none'; base-uri 'self'; form-action 'none'; frame-src 'self'"
ASSETS = ('index.html', 'jtml-runtime.js', 'components/index.js', 'app.js')


def digest(file):
    return hashlib.sha256(file.read_bytes()).hexdigest()


def load_page_registry():
    registry = json.loads((ROOT / 'site/pages.json').read_text())
    if set(registry) != {'schema', 'pages'} or registry['schema'] != 1:
        raise ValueError('Unsupported page registry')
    required = {'id', 'entry', 'output', 'assetDirectory', 'css', 'title', 'description'}
    ids, entries, outputs, directories = set(), set(), set(), set()
    for page in registry['pages']:
        if not required <= set(page) or set(page) - required - {'themeColor'}:
            raise ValueError('Invalid page registry fields')
        if not re.fullmatch(r'[a-z]+', page['id']) or not re.fullmatch(r'[a-z]+\.jtml', page['entry']) or not re.fullmatch(r'[a-z]+\.html', page['output']):
            raise ValueError('Page identifiers must be local filenames')
        directory = page['assetDirectory']
        if directory != '.' and not re.fullmatch(r'[a-z]+-assets', directory):
            raise ValueError('Invalid bundle directory')
        if page['id'] in ids or page['entry'] in entries or page['output'] in outputs or directory in directories:
            raise ValueError('Page registry output collision')
        ids.add(page['id'])
        entries.add(page['entry'])
        outputs.add(page['output'])
        directories.add(directory)
        if not isinstance(page['css'], list) or not page['css']:
            raise ValueError('Each page needs CSS assets')
        for css in page['css']:
            if not re.fullmatch(r'[a-z]+\.css', css) or not (ROOT / 'site' / css).is_file():
                raise ValueError('Invalid or missing CSS asset: ' + css)
        for field in ('title', 'description'):
            if not isinstance(page[field], str) or not page[field].strip():
                raise ValueError('Missing page metadata: ' + field)
        if 'themeColor' in page and not re.fullmatch(r'#[0-9a-fA-F]{6}', page['themeColor']):
            raise ValueError('Invalid theme color')
        if not (ROOT / 'site' / page['entry']).is_file():
            raise ValueError('Missing JTML entry: ' + page['entry'])
    if len(registry['pages']) != 6 or outputs != {'index.html', 'reference.html', 'examples.html', 'tools.html', 'security.html', 'deploy.html'}:
        raise ValueError('The registry must describe the six existing public URLs')
    if next(page for page in registry['pages'] if page['output'] == 'index.html')['assetDirectory'] != '.':
        raise ValueError('Preserve homepage root assets')
    return registry


def payload_report(output, registry):
    def sizes(data):
        return dict(raw=len(data), gzip=len(gzip.compress(data, compresslevel=9, mtime=0)))

    pages = {}
    for page in registry['pages']:
        name, directory, styles = page['output'], page['assetDirectory'], page['css']
        paths = [name] + [(Path(directory) / asset).as_posix() for asset in ASSETS[1:]] + styles
        files = {path: sizes((output / path).read_bytes()) for path in paths}
        markup = (output / name).read_text()
        manifests = re.findall(r'<script[^>]*type="application/json"[^>]*>(.*?)</script>', markup, re.S)
        manifest = json.loads(manifests[0])
        occurrences = {}
        def visit(value):
            if isinstance(value, dict):
                for key, child in value.items():
                    if key == 'bodyPlan':
                        encoded = json.dumps(child, sort_keys=True, separators=(',', ':'), ensure_ascii=False)
                        occurrences[encoded] = occurrences.get(encoded, 0) + 1
                    visit(child)
            elif isinstance(value, list):
                for child in value:
                    visit(child)
        visit(manifest)
        repeated = sum(len(value.encode()) * (count - 1) for value, count in occurrences.items())
        pages[name] = dict(files=files,
            byType={suffix: {kind: sum(size[kind] for path, size in files.items() if path.endswith('.' + suffix))
                             for kind in ('raw', 'gzip')} for suffix in ('html', 'js', 'css')},
            total={kind: sum(item[kind] for item in files.values()) for kind in ('raw', 'gzip')},
            generatedData=dict(clientManifest=sizes(manifests[0].encode()),
                bodyPlanOccurrences=sum(occurrences.values()),
                exactRepeatedBodyPlanBytes=repeated))
    runtimes = [output / page['assetDirectory'] / 'jtml-runtime.js' for page in registry['pages']]
    runtime_equal = len({digest(file) for file in runtimes}) == 1
    runtime_size = sizes(runtimes[0].read_bytes())
    return dict(schema=2, compression='gzip level 9, mtime 0, each asset independently; estimates, not hosted transfer headers',
                zlibVersion=zlib.ZLIB_RUNTIME_VERSION,
                pages=pages, crossPage=dict(identicalRuntimeCopies=runtime_equal, runtimeCopies=len(runtimes),
                runtimeBytesPerCopy=runtime_size if runtime_equal else None,
                additionalRuntimeBytes={kind: value * (len(runtimes) - 1) for kind, value in runtime_size.items()} if runtime_equal else None),
                accounting='Repeated bodyPlan arrays use canonical compact JSON bytes beyond the first identical occurrence; not a proposed removable-byte budget.')


def build(output, compiler, base_url):
    registry = load_page_registry()
    if any((ROOT / 'site').rglob('*.html')):
        raise ValueError('Handwritten page HTML is not a website source input')
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
        compiled = {}
        for page in registry['pages']:
            entry, temporary_name = page['entry'], page['id']
            destination = output / page['assetDirectory']
            source_dir = work / (temporary_name + '-source')
            source_dir.mkdir()
            shutil.copyfile(ROOT / 'site' / entry, source_dir / entry)
            shutil.copytree(ROOT / 'site/shared', source_dir / 'shared')
            subprocess.run([compiler, 'build', temporary_name + '-source/' + entry,
                            '--target', 'browser', '--out', temporary_name], cwd=work, check=True)
            bundle = work / temporary_name
            assert {p.relative_to(bundle).as_posix() for p in bundle.rglob('*') if p.is_file()} == set(ASSETS)
            hashes = {name: digest(bundle / name) for name in ASSETS}
            for name in ASSETS:
                content = (bundle / name).read_text()
                assert not re.search(r'\bnew\s+Function\b|\beval\s*\(', content), name
                assert 'jtml-update-plans.js' not in content, name
            destination.mkdir(exist_ok=True)
            for name in ASSETS[1:]:
                target = destination / name
                target.parent.mkdir(exist_ok=True)
                shutil.copyfile(bundle / name, target)
            page_name = page['output']
            shutil.copyfile(bundle / 'index.html', output / page_name)
            compiled[page_name] = dict(source='site/' + entry,
                sourceInputsSHA256={name: digest(ROOT / name) for name in
                                   ['site/' + entry] + sorted(file.relative_to(ROOT).as_posix() for file in (ROOT / 'site/shared').rglob('*.jtml'))},
                generatedAssetsSHA256=hashes,
                assetDirectory=page['assetDirectory'], metadata=page)
        homepage_hashes = compiled['index.html']['generatedAssetsSHA256']
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
        icon = '../../favicon.svg' if page == demo / 'index.html' else './favicon.svg'
        additions += '<link rel="icon" type="image/svg+xml" href="' + icon + '">\n'
        location = page.relative_to(output).as_posix()
        canonical = base_url + ('' if location == 'index.html' else location.replace('demo/counter/index.html', 'demo/counter/'))
        additions += '<link rel="canonical" href="' + html.escape(canonical, quote=True) + '">\n'
        text = text.replace('<head>', '<head>\n' + additions, 1)
        if page.parent == output and page.name in compiled:
            # Packaging metadata and external script URLs only; application markup and JavaScript stay intact.
            text = text.replace('<html>', '<html lang="en">', 1)
            metadata = compiled[page.name]['metadata']
            text = re.sub(r'<title>.*?</title>', lambda _: '<title>' + html.escape(metadata['title']) + '</title>', text, count=1)
            prefix = metadata['assetDirectory']
            for name in ASSETS[1:]:
                original = 'src="./' + name + '"'
                assert text.count(original) == 1
                text = text.replace(original, 'src="./' + ('' if prefix == '.' else prefix + '/') + name + '"', 1)
            head = '<meta name="description" content="' + html.escape(metadata['description'], quote=True) + '">\n'
            head += '<meta name="viewport" content="width=device-width, initial-scale=1">\n'
            head += ''.join('<link rel="stylesheet" href="./' + css + '">\n' for css in metadata['css'])
            if 'themeColor' in metadata:
                head += '<meta name="theme-color" content="' + html.escape(metadata['themeColor'], quote=True) + '">\n'
            text = text.replace('</head>', head + '</head>', 1)
        page.write_text(text)
    for name in ASSETS[1:]:
        assert digest(demo / name) == original_hashes[name], 'Generated JavaScript must remain untouched'
    for page_info in compiled.values():
        for name in ASSETS[1:]:
            assert digest(output / page_info['assetDirectory'] / name) == page_info['generatedAssetsSHA256'][name], 'Generated page JavaScript must remain untouched'
    locations = [base_url + ('' if p.name == 'index.html' else p.name) for p in sorted(output.glob('*.html'))]
    locations.append(base_url + 'demo/counter/')
    (output / 'sitemap.xml').write_text('<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n' + ''.join('  <url><loc>' + escape(u) + '</loc></url>\n' for u in locations) + '</urlset>\n')
    (output / 'robots.txt').write_text('User-agent: *\nAllow: /\n\nSitemap: ' + base_url + 'sitemap.xml\n')
    revision = subprocess.check_output(['git', '-C', ROOT, 'rev-parse', 'HEAD'], text=True).strip()
    dirty = bool(subprocess.check_output(['git', '-C', ROOT, 'status', '--porcelain', '--untracked-files=normal'], text=True))
    info = dict(schema=4, registrySHA256=digest(ROOT / "site/pages.json"), pages=compiled, packagingPolicy="Registry document metadata and bundle script URLs only; application markup and generated JS are unchanged", compiler=compiler_info, homepageSource='site/index.jtml', homepageSourceSHA256=digest(ROOT / 'site/index.jtml'), homepageGeneratedAssetsSHA256=homepage_hashes, sourceRevision=revision, sourceDirty=dirty, compilerVersion=version,
                baseURL=base_url, counterSource='examples/friendly_counter.jtml', counterSourceSHA256=digest(source),
                generatedAssetsSHA256=original_hashes, demoPolicy='Only document head/language metadata is added to compiled HTML; body and generated JavaScript are unchanged')
    (output / 'site-build.json').write_text(json.dumps(info, sort_keys=True, indent=2) + '\n')
    (output / 'site-payloads.json').write_text(json.dumps(payload_report(output, registry), sort_keys=True, indent=2) + '\n')
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
