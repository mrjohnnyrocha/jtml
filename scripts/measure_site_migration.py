#!/usr/bin/env python3
"""Compare complete deployed site inventories and cold/warm page resource sets."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import zlib


def sizes(data):
    return dict(raw=len(data), gzip=len(gzip.compress(data, compresslevel=9, mtime=0)))


def total(files):
    return {kind: sum(size[kind] for size in files.values()) for kind in ('raw', 'gzip')}


def measure(root):
    root = root.resolve()
    expected_files = {'manifest.txt'}
    for line in (root / 'manifest.txt').read_text().splitlines():
        digest, name = line.split('  ', 1)
        if Path(name).is_absolute() or '..' in Path(name).parts:
            raise ValueError('Invalid deployment manifest path')
        expected_files.add(name)
        path = root / name
        if not path.is_file():
            raise ValueError('Complete deployed inventory required; missing ' + name)
        if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise ValueError('Deployment manifest mismatch: ' + name)
    inventory = {p.relative_to(root).as_posix(): sizes(p.read_bytes()) for p in sorted(root.rglob('*')) if p.is_file()}
    if set(inventory) != expected_files:
        raise ValueError('Deployment file inventory differs from manifest')
    info = json.loads((root / 'site-build.json').read_text())
    registry = json.loads((root / 'pages.json').read_text())
    pages = {}
    for page in registry['pages']:
        paths = [page['output']] + page['css'] + ['favicon.svg']
        if page.get('target', 'browser') == 'browser':
            paths += ['jtml-runtime.js'] + [(Path(page['assetDirectory']) / p).as_posix() for p in ('components/index.js', 'app.js')]
        pages[page['output']] = {path: inventory[path] for path in paths}
    homepage = set(pages['index.html'])
    warm = {name: total({p: s for p, s in files.items() if p == name or p not in homepage}) for name, files in pages.items()}
    seen, sequential = set(), {}
    for name, files in pages.items():
        sequential[name] = total({p: s for p, s in files.items() if p == name or p not in seen})
        seen.update(files)
    return dict(sourceRevision=info['sourceRevision'], sourceDirty=info['sourceDirty'], compiler=info['compiler'],
                coldPages={name: total(files) for name, files in pages.items()}, afterHomepage=warm,
                sequentialNavigation=sequential, uniquePageResources=total({p: s for f in pages.values() for p, s in f.items()}),
                completeDeployment=dict(files=len(inventory), total=total(inventory)))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('before', type=Path)
    parser.add_argument('after', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    before, after = measure(args.before), measure(args.after)
    result = dict(compression='gzip level 9, mtime 0, every file independently; estimates, not hosted transfer headers',
                  zlib=zlib.ZLIB_RUNTIME_VERSION,
                  navigationAssumption='Fresh target HTML; reusable previously visited resources cached. Sequential order follows registry.',
                  before=before, after=after,
                  savings={kind: before['completeDeployment']['total'][kind] - after['completeDeployment']['total'][kind] for kind in ('raw', 'gzip')})
    args.output.write_text(json.dumps(result, sort_keys=True, indent=2) + '\n')
    print(json.dumps(result, sort_keys=True, indent=2))
