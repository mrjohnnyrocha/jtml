#!/usr/bin/env python3
"""Independent archive byte, path and mutation controls; no compiler required."""
import hashlib
import importlib.util
from pathlib import Path
import sys
import tarfile
import tempfile

# Importing the packaging helper must not dirty an otherwise clean checkout.
sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('pack', sys.argv[1])
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)
with tempfile.TemporaryDirectory() as directory:
    temp = Path(directory)
    root = temp / 'release'
    (root / 'bin').mkdir(parents=True)
    (root / 'bin/jtml').write_text('executable fixture\n')
    (root / 'bin/jtml').chmod(0o755)
    (root / 'LICENSE').write_text('license fixture\n')
    args = ('0.3.0-dev', 'test', 'fixture', 'source-revision', False, 1000000000)
    first, second = temp / 'first.tar.gz', temp / 'second.tar.gz'
    pack.create(root, first, *args)
    pack.create(root, second, *args)
    assert first.read_bytes() == second.read_bytes(), 'Repeated packaging differs'
    with tarfile.open(first) as archive:
        names = archive.getnames()
        assert names == sorted(names)
        assert all(not Path(name).is_absolute() and '..' not in Path(name).parts for name in names)
        assert all(member.mtime == 1000000000 and member.uid == member.gid == 0 for member in archive)
        assert archive.getmember('release/bin/jtml').mode == 0o755
        manifest = archive.extractfile('release/MANIFEST.txt').read().decode().splitlines()
        assert manifest == ['ARTIFACT.json', 'LICENSE', 'MANIFEST.txt', 'bin/jtml'], manifest
        for line in archive.extractfile('release/SHA256SUMS').read().decode().splitlines():
            digest, name = line.split('  ', 1)
            assert hashlib.sha256(archive.extractfile('release/' + name).read()).hexdigest() == digest
    (root / 'LICENSE').write_text('changed independent fixture\n')
    pack.create(root, second, *args)
    assert first.read_bytes() != second.read_bytes(), 'Mutated artifact retained checksum'
print('PASS: deterministic archive, relative manifest, normalized metadata, executable permissions and changed-content control')
