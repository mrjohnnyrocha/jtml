#!/usr/bin/env python3
"""Create a deterministic archive from one prepared CLI distribution tree."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import tarfile


def create(root, archive, version, platform, architecture, revision, dirty, epoch):
    metadata = dict(schema=1, version=version, platform=platform, architecture=architecture,
                    sourceRevision=revision, sourceDirty=dirty, sourceDateEpoch=epoch)
    (root / 'ARTIFACT.json').write_text(json.dumps(metadata, sort_keys=True, indent=2) + '\n')
    files = sorted(p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file())
    files = sorted(set(files) - {'SHA256SUMS', 'MANIFEST.txt'} | {'MANIFEST.txt'})
    (root / 'MANIFEST.txt').write_text(''.join(name + '\n' for name in files))
    (root / 'SHA256SUMS').write_text(''.join(
        hashlib.sha256((root / name).read_bytes()).hexdigest() + '  ' + name + '\n'
        for name in files))
    # Normalize container metadata; binary reproducibility across toolchains is
    # a separate claim. The same prepared tree and epoch yield identical bytes.
    with archive.open('wb') as output, gzip.GzipFile(filename='', mode='wb', fileobj=output, mtime=epoch) as zipped:
        with tarfile.open(fileobj=zipped, mode='w', format=tarfile.PAX_FORMAT) as tar:
            for path in [root, *sorted(root.rglob('*'))]:
                if path.is_symlink():
                    raise ValueError('Unexpected symlink in release tree: ' + str(path))
                info = tar.gettarinfo(str(path), arcname=str(Path(root.name) / path.relative_to(root)))
                info.uid = info.gid = 0
                info.uname = info.gname = ''
                info.mtime = epoch
                info.mode = 0o755 if path.is_dir() or path.stat().st_mode & 0o111 else 0o644
                info.pax_headers = {}
                if path.is_file():
                    with path.open('rb') as content:
                        tar.addfile(info, content)
                else:
                    tar.addfile(info)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    parser.add_argument('archive', type=Path)
    parser.add_argument('--version', required=True)
    parser.add_argument('--platform', required=True)
    parser.add_argument('--architecture', required=True)
    parser.add_argument('--revision', required=True)
    parser.add_argument('--dirty', choices=('true', 'false'), required=True)
    parser.add_argument('--epoch', type=int, required=True)
    args = parser.parse_args()
    create(args.root, args.archive, args.version, args.platform, args.architecture,
           args.revision, args.dirty == 'true', args.epoch)
