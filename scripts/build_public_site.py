#!/usr/bin/env python3
"""Compile the JTML public pages and original Counter; never deploy anything."""
import argparse
import gzip
import hashlib
import html
import json
import os
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
    if set(registry) != {'schema', 'sharedRuntime', 'pages'} or registry['schema'] != 3:
        raise ValueError('Unsupported page registry')
    if registry['sharedRuntime'] != 'jtml-runtime.js':
        raise ValueError('Shared runtime must stay at the website root')
    required = {'id', 'entry', 'output', 'assetDirectory', 'css', 'title', 'description', 'target'}
    ids, entries, outputs, directories = set(), set(), set(), set()
    for page in registry['pages']:
        if not required <= set(page) or set(page) - required - {'themeColor'}:
            raise ValueError('Invalid page registry fields')
        if not re.fullmatch(r'[a-z]+', page['id']) or not re.fullmatch(r'[a-z]+\.jtml', page['entry']) or not re.fullmatch(r'[a-z]+\.html', page['output']):
            raise ValueError('Page identifiers must be local filenames')
        directory = page['assetDirectory']
        if page['target'] not in ('static', 'browser'):
            raise ValueError('Invalid page target')
        if page['target'] == 'static' and directory is not None:
            raise ValueError('Static pages cannot own JavaScript asset directories')
        if page['target'] == 'browser' and directory != '.' and (not isinstance(directory, str) or not re.fullmatch(r'[a-z]+-assets', directory)):
            raise ValueError('Invalid bundle directory')
        if page['id'] in ids or page['entry'] in entries or page['output'] in outputs or (directory is not None and directory in directories):
            raise ValueError('Page registry output collision')
        ids.add(page['id'])
        entries.add(page['entry'])
        outputs.add(page['output'])
        if directory is not None:
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
    if {page['output'] for page in registry['pages'] if page['target'] == 'browser'} != {'index.html', 'reference.html'}:
        raise ValueError('Only Homepage and Reference must retain the browser target')
    return registry


def page_assets(page):
    return ASSETS if page['target'] == 'browser' else ('index.html',)


def deployed_page_assets(page, registry):
    assets = [page['output']]
    if page['target'] == 'browser':
        assets += [registry['sharedRuntime']] + [(Path(page['assetDirectory']) / asset).as_posix() for asset in ASSETS[2:]]
    return assets


def verify_shared_runtime(bundles):
    # Compare actual bytes, not just hashes, before removing any deployed copy.
    if len(bundles) < 2:
        raise ValueError('Runtime equality requires both browser page bundles')
    original = (next(iter(bundles.values())) / 'jtml-runtime.js').read_bytes()
    for page, bundle in bundles.items():
        if (bundle / 'jtml-runtime.js').read_bytes() != original:
            raise ValueError('Compiled page runtimes differ: ' + page + '; refusing runtime sharing')


def payload_report(output, registry):
    def sizes(data):
        return dict(raw=len(data), gzip=len(gzip.compress(data, compresslevel=9, mtime=0)))

    pages = {}
    for page in registry['pages']:
        name, directory, styles = page['output'], page['assetDirectory'], page['css']
        paths = deployed_page_assets(page, registry) + styles + ['favicon.svg']
        files = {path: sizes((output / path).read_bytes()) for path in paths}
        markup = (output / name).read_text()
        manifests = re.findall(r'<script[^>]*type="application/json"[^>]*>(.*?)</script>', markup, re.S)
        manifest = json.loads(manifests[0]) if manifests else {}
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
        pages[name] = dict(target=page['target'], files=files,
            byType={suffix: {kind: sum(size[kind] for path, size in files.items() if path.endswith('.' + suffix))
                             for kind in ('raw', 'gzip')} for suffix in ('html', 'js', 'css')},
            total={kind: sum(item[kind] for item in files.values()) for kind in ('raw', 'gzip')},
            generatedData=dict(clientManifest=sizes(manifests[0].encode()) if manifests else dict(raw=0, gzip=0),
                bodyPlanOccurrences=sum(occurrences.values()),
                exactRepeatedBodyPlanBytes=repeated))
    runtime_size = sizes((output / registry['sharedRuntime']).read_bytes())
    deployed = {file.relative_to(output).as_posix(): sizes(file.read_bytes())
                for file in sorted(output.rglob('*')) if file.is_file()
                and file.name not in ('site-build.json', 'site-payloads.json', 'manifest.txt')}
    home_files = set(pages['index.html']['files'])
    warm = {name: {kind: sum(size[kind] for path, size in page['files'].items() if path == name or path not in home_files)
                   for kind in ('raw', 'gzip')} for name, page in pages.items()}
    union = {path: size for page in pages.values() for path, size in page['files'].items()}
    return dict(schema=4, compression='gzip level 9, mtime 0, each asset independently; estimates, not hosted transfer headers',
                zlibVersion=zlib.ZLIB_RUNTIME_VERSION, pages=pages,
                navigation=dict(assumption='Fresh target HTML; reusable homepage resources cached. Resource-set estimate, not hosted cache guarantees.', afterHomepage=warm),
                uniquePageResources={kind: sum(size[kind] for size in union.values()) for kind in ('raw', 'gzip')},
                deployedAssets=dict(fileCount=len(deployed), total={kind: sum(size[kind] for size in deployed.values()) for kind in ('raw', 'gzip')},
                    exclusions=['site-build.json', 'site-payloads.json', 'manifest.txt']),
                crossPage=dict(identicalCompiledRuntimes=True, compiledRuntimeCopies=sum(page['target'] == 'browser' for page in registry['pages']),
                    deployedRuntimeCopies=1, runtimeBytesPerCopy=runtime_size,
                    removedRuntimeBytes={kind: value * (sum(page['target'] == 'browser' for page in registry['pages']) - 1) for kind, value in runtime_size.items()}),
                accounting='Cold browser pages include one complete runtime; static pages include no JavaScript. Cold totals include favicon. Deployed assets count each path once, including docs, source and standalone Counter. Metadata reports excluded to avoid self-reference. Repeated bodyPlan arrays are observations, not removable-byte budgets.')


def _build(output, compiler, base_url):
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
        originals = output.with_name(output.name + '.compiler-output')
        if originals.exists():
            shutil.rmtree(originals)
        originals.mkdir(parents=True)
        compiled = {}
        bundles = {}
        for page in registry['pages']:
            entry, temporary_name = page['entry'], page['id']
            source_dir = work / (temporary_name + '-source')
            source_dir.mkdir()
            shutil.copyfile(ROOT / 'site' / entry, source_dir / entry)
            shutil.copytree(ROOT / 'site/shared', source_dir / 'shared')
            subprocess.run([compiler, 'build', temporary_name + '-source/' + entry,
                            '--target', page['target'], '--out', temporary_name], cwd=work, check=True)
            bundle = work / temporary_name
            if {p.relative_to(bundle).as_posix() for p in bundle.rglob('*') if p.is_file()} != set(page_assets(page)):
                raise ValueError('Compiler target inventory mismatch for site/' + entry + ' --target ' + page['target'])
            preserved = originals / page['id']
            shutil.copytree(bundle, preserved)
            if page['target'] == 'browser':
                bundles[page['output']] = preserved
            hashes = {name: digest(bundle / name) for name in page_assets(page)}
            for name in page_assets(page):
                content = (bundle / name).read_text()
                assert not re.search(r'\bnew\s+Function\b|\beval\s*\(', content), name
                assert 'jtml-update-plans.js' not in content, name
            if page['target'] == 'static':
                markup = (bundle / 'index.html').read_text()
                if re.search(r'<script\b|data-jtml-|__jtml_client_manifest', markup, re.I):
                    raise ValueError('Static compiler output contains runtime payload: ' + page['output'])
            page_name = page['output']
            compiled[page_name] = dict(target=page['target'], source='site/' + entry,
                sourceInputsSHA256={name: digest(ROOT / name) for name in
                                   ['site/' + entry] + sorted(file.relative_to(ROOT).as_posix() for file in (ROOT / 'site/shared').rglob('*.jtml'))},
                generatedAssetsSHA256=hashes,
                assetDirectory=page['assetDirectory'], metadata=page)
        verify_shared_runtime(bundles)
        # Only publish assets once all six originals have passed the equality gate.
        if output.exists():
            shutil.rmtree(output)
        shutil.copytree(ROOT / 'site', output, ignore=shutil.ignore_patterns('CNAME'))
        for page in registry['pages']:
            bundle = originals / page['id']
            if page['target'] == 'static':
                shutil.copyfile(bundle / 'index.html', output / page['output'])
                continue
            destination = output / page['assetDirectory']
            destination.mkdir(exist_ok=True)
            for name in ASSETS[1:]:
                if name == registry['sharedRuntime'] and page['assetDirectory'] != '.':
                    continue
                target = destination / name
                target.parent.mkdir(exist_ok=True)
                shutil.copyfile(bundle / name, target)
            shutil.copyfile(bundle / 'index.html', output / page['output'])
        homepage_hashes = compiled['index.html']['generatedAssetsSHA256']
        demo = output / 'demo/counter'
        shutil.copytree(generated, originals / 'counter')
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
        is_static = page.name in compiled and compiled[page.name]['target'] == 'static'
        if is_static:
            policies = re.findall(r'<meta http-equiv="Content-Security-Policy"[^>]*>', text)
            required_policy = ("script-src 'none'", "connect-src 'none'", "object-src 'none'", "base-uri 'none'")
            if len(policies) != 1 or any(directive not in html.unescape(policies[0]) for directive in required_policy):
                raise ValueError('Static compiler output must retain strict CSP')
        additions = '' if is_static else '<meta http-equiv="Content-Security-Policy" content="' + html.escape(CSP, quote=True) + '">\n'
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
            for name in (() if is_static else ASSETS[1:]):
                original = 'src="./' + name + '"'
                assert text.count(original) == 1
                text = text.replace(original, 'src="./' + ('' if name == registry['sharedRuntime'] or prefix == '.' else prefix + '/') + name + '"', 1)
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
        for name in (() if page_info['target'] == 'static' else ASSETS[1:]):
            assert digest(output / ('' if name == registry['sharedRuntime'] else page_info['assetDirectory']) / name) == page_info['generatedAssetsSHA256'][name], 'Generated page JavaScript must remain untouched'
    locations = [base_url + ('' if p.name == 'index.html' else p.name) for p in sorted(output.glob('*.html'))]
    locations.append(base_url + 'demo/counter/')
    (output / 'sitemap.xml').write_text('<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n' + ''.join('  <url><loc>' + escape(u) + '</loc></url>\n' for u in locations) + '</urlset>\n')
    (output / 'robots.txt').write_text('User-agent: *\nAllow: /\n\nSitemap: ' + base_url + 'sitemap.xml\n')
    revision = subprocess.check_output(['git', '-C', ROOT, 'rev-parse', 'HEAD'], text=True).strip()
    dirty = bool(subprocess.check_output(['git', '-C', ROOT, 'status', '--porcelain', '--untracked-files=normal'], text=True))
    for page in registry['pages']:
        compiled[page['output']]['deployedAssetsSHA256'] = {name: digest(output / name) for name in deployed_page_assets(page, registry)}
    info = dict(schema=6, sharedRuntime=dict(path=registry['sharedRuntime'], verifiedByteEqualPages=sorted(bundles), sha256=digest(output / registry['sharedRuntime'])), originalCompilerOutputSuffix='.compiler-output', registrySHA256=digest(ROOT / "site/pages.json"), pages=compiled, packagingPolicy="Registry-selected compiler targets; document metadata, browser script URLs and byte-equal runtime sharing only; application markup and generated JS are unchanged", compiler=compiler_info, homepageSource='site/index.jtml', homepageSourceSHA256=digest(ROOT / 'site/index.jtml'), homepageGeneratedAssetsSHA256=homepage_hashes, sourceRevision=revision, sourceDirty=dirty, compilerVersion=version,
                baseURL=base_url, counterSource='examples/friendly_counter.jtml', counterSourceSHA256=digest(source),
                generatedAssetsSHA256=original_hashes, demoPolicy='Only document head/language metadata is added to compiled HTML; body and generated JavaScript are unchanged')
    (output / 'site-build.json').write_text(json.dumps(info, sort_keys=True, indent=2) + '\n')
    (output / 'site-payloads.json').write_text(json.dumps(payload_report(output, registry), sort_keys=True, indent=2) + '\n')
    files = sorted(p for p in output.rglob('*') if p.is_file() and p != output / 'manifest.txt')
    (output / 'manifest.txt').write_text(''.join(digest(p) + '  ' + p.relative_to(output).as_posix() + '\n' for p in files))
    print('Built static site (no deployment):', output)


def build(output, compiler, base_url):
    output = Path(output).absolute()
    if output.is_symlink() or output.with_name(output.name + '.compiler-output').is_symlink():
        raise ValueError('Website output and provenance destinations must not be symlinks')
    output = output.resolve()
    if output == ROOT or ROOT.is_relative_to(output) or (output.is_relative_to(ROOT) and
            (not output.is_relative_to(ROOT / 'dist') or output == ROOT / 'dist')):
        raise ValueError('Output must not replace the repository or site source')
    # Validate before starting compiler work or changing an accepted artifact.
    load_page_registry()
    if any((ROOT / 'site').rglob('*.html')):
        raise ValueError('Handwritten page HTML is not a website source input')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.jtml-site-stage-', dir=output.parent) as temporary:
        staged = Path(temporary) / output.name
        try:
            _build(staged, compiler, base_url)
        except Exception:
            failed = staged.with_name(staged.name + '.compiler-output')
            if failed.exists():
                diagnostics = output.with_name(output.name + '.failed-compiler-output')
                if diagnostics.exists():
                    shutil.rmtree(diagnostics)
                shutil.copytree(failed, diagnostics)
            raise
        destinations = [output, output.with_name(output.name + '.compiler-output')]
        sources = [staged, staged.with_name(staged.name + '.compiler-output')]
        backups, published = {}, []
        try:
            for i, dest in enumerate(destinations):
                if dest.exists():
                    backups[dest] = Path(temporary) / ('previous-' + str(i))
                    os.replace(dest, backups[dest])
            for source, dest in zip(sources, destinations):
                os.replace(source, dest)
                published.append(dest)
        except Exception:
            for dest in published:
                shutil.rmtree(dest)
            for dest, backup in backups.items():
                os.replace(backup, dest)
            raise
    print('Published local website artifact and compiler originals (no deployment):', output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--base-url', required=True)
    args = parser.parse_args()
    build(args.output, args.compiler, args.base_url)
