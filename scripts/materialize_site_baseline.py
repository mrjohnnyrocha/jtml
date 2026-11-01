#!/usr/bin/env python3
"""Materialize the immutable, manifest-verified published rendering fixture."""
import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path
import tarfile

ROOT = Path(__file__).resolve().parents[1]


def materialize(output):
    archive = ROOT / 'tests/fixtures/site/published-baseline.tar.gz'
    expected = json.loads(archive.with_name('published-baseline.json').read_text())
    compressed = archive.read_bytes()
    assert hashlib.sha256(compressed).hexdigest() == expected['archiveSHA256']
    files = {}
    with tarfile.open(fileobj=io.BytesIO(gzip.decompress(compressed)), mode='r:') as source:
        for member in source:
            name = Path(member.name)
            if not member.isfile() or name.is_absolute() or '..' in name.parts or member.name in files:
                raise ValueError('Invalid published baseline archive entry')
            files[member.name] = source.extractfile(member).read()
    info = json.loads(files['site-build.json'])
    assert info['sourceRevision'] == expected['sourceRevision']
    assert info['compiler']['revision'] == expected['compilerRevision']
    hashes = {name: digest for digest, name in (line.split('  ', 1) for line in files['manifest.txt'].decode().splitlines())}
    for name, data in files.items():
        if name != 'manifest.txt':
            assert hashlib.sha256(data).hexdigest() == hashes[name], name
    output.mkdir(parents=True, exist_ok=True)
    for name, data in files.items():
        target = output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    print('Verified published baseline:', expected['sourceRevision'], output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    materialize(parser.parse_args().output)
