#!/usr/bin/env python3
"""Prepare review-only 0.3.0-rc.2 artifacts using the existing JTML packagers."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tempfile

VERSION = '0.3.0-rc.2'


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()


def require_clean_revision(root, expected):
    if not re.fullmatch(r'[0-9a-fA-F]{40}', expected):
        raise ValueError('Use the full 40-character reviewed commit SHA, not a moving ref')
    if git(root, 'rev-parse', 'HEAD') != expected.lower():
        raise ValueError('Checkout does not match the selected source revision')
    if git(root, 'status', '--porcelain', '--untracked-files=normal'):
        raise ValueError('RC preparation requires a clean source checkout')


def validate_metadata(metadata, revision, system, arch, epoch):
    expected = dict(version=VERSION, sourceRevision=revision.lower(), sourceDirty=False,
                    platform=system, architecture=arch, sourceDateEpoch=epoch)
    for key, value in expected.items():
        if metadata.get(key) != value:
            raise ValueError(f'Artifact {key}: expected {value!r}, got {metadata.get(key)!r}')


def prepare(root, revision, build):
    require_clean_revision(root, revision)
    revision = revision.lower()
    system, arch = platform.system().lower(), platform.machine()
    if (system, arch) not in {('linux', 'x86_64'), ('darwin', 'arm64')}:
        raise ValueError('RC.2 targets Ubuntu x86_64 and macOS arm64; other targets are unverified')
    epoch = int(git(root, 'show', '-s', '--format=%ct', 'HEAD'))
    env = {**os.environ, 'JTML_VERSION_SUFFIX': 'rc.2', 'BUILD_TYPE': 'Release',
           'BUILD_DIR': str(build), 'SOURCE_DATE_EPOCH': str(epoch), 'PYTHONDONTWRITEBYTECODE': '1'}

    if system == 'darwin':
        if env.get('MACOSX_DEPLOYMENT_TARGET', '14.0') != '14.0':
            raise ValueError('RC macOS minimum deployment target must be 14.0')
        env['MACOSX_DEPLOYMENT_TARGET'] = '14.0'

    def run(*args):
        subprocess.run(list(map(str, args)), cwd=root, env=env, check=True)

    # Keep the existing packager and strict distribution tests as the owners.
    run(root / 'scripts/package_cli.sh', root / 'dist/release')
    binary = root / 'dist/release/bin/jtml'
    actual = subprocess.check_output([str(binary), '--version'], env=env, text=True).strip()
    if actual != 'jtml ' + VERSION:
        raise ValueError('Refusing incorrectly versioned RC binary: ' + actual)
    metadata = json.loads((root / 'dist/release/ARTIFACT.json').read_text())
    validate_metadata(metadata, revision, system, arch, epoch)
    bundle = root / f'dist/jtml-{VERSION}-{system}-{arch}.tar.gz'
    install_name = f'jtml-{VERSION}-{system}-{arch}-install.tar.gz'
    if (build / 'jtml-cpack-filename.txt').read_text().strip() != install_name:
        raise ValueError('CPack version/platform identity differs from the selected RC')
    run('cpack', '--config', build / 'CPackConfig.cmake', '-B', root / 'dist/cpack')
    cpack = root / 'dist/cpack' / install_name
    with tempfile.TemporaryDirectory(prefix='jtml-rc-install-') as prefix:
        # CPack rewrites install_manifest.txt: the final explicit install is last.
        run('cmake', '--install', build, '--prefix', prefix)
        run('python3', root / 'scripts/test_studio_distribution.py', '--build-dir', build,
            '--archive', bundle, '--cpack-archive', cpack, '--install-prefix', prefix)
    macos_audits = []
    if system == 'darwin':
        # Audit what testers extract, including CPack, not just build outputs.
        for archive in (bundle, cpack):
            with tempfile.TemporaryDirectory(prefix='jtml-macho-') as extracted:
                run('tar', '-xzf', archive, '-C', extracted)
                report = Path(extracted) / 'audit.json'
                run('python3', root / 'scripts/check_macos_distribution.py',
                    next(Path(extracted).iterdir()), '--output', report)
                macos_audits.append(dict(archive=archive.name,
                                        **json.loads(report.read_text())))
    require_clean_revision(root, revision)
    output = root / 'dist/rc-review'
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    for file in (bundle, cpack, root / 'dist/release/ARTIFACT.json'):
        shutil.copy2(file, output / file.name)
    shutil.copy2(root / 'docs/releases/v0.3.0-rc.2.md', output / 'RELEASE_NOTES.md')
    shutil.copy2(root / 'docs/releases/rc-feedback.md', output / 'FEEDBACK.md')
    report = dict(version=VERSION, sourceRevision=revision, sourceDirty=False,
                  platform=system, architecture=arch, host=platform.platform(),
                  checks=['bundle', 'cmake-install', 'cpack', 'compiled-c-api', 'studio-resources'],
                  approvalStatus='not-recorded-by-this-command', artifacts=[])
    for file in (bundle, cpack):
        report['artifacts'].append(dict(file=file.name, bytes=file.stat().st_size,
                                       sha256=hashlib.sha256(file.read_bytes()).hexdigest()))
    (output / 'RC_REVIEW.json').write_text(json.dumps(report, sort_keys=True, indent=2) + '\n')
    with (output / 'BUILD_ENVIRONMENT.txt').open('w') as evidence:
        for command in (['uname', '-a'], ['cmake', '--version'], ['c++', '--version']):
            subprocess.run(command, env=env, check=True, stdout=evidence, stderr=subprocess.STDOUT)
        libraries = list((root / 'dist/release/lib').glob('libjtml.*'))
        for file in [binary, *libraries]:
            command = ['otool', '-L', str(file)] if system == 'darwin' else ['ldd', str(file)]
            subprocess.run(command, env=env, check=True, stdout=evidence, stderr=subprocess.STDOUT)
    if macos_audits:
        (output / 'MACOS_COMPATIBILITY.json').write_text(
            json.dumps(macos_audits, sort_keys=True, indent=2) + '\n')
    files = sorted(p for p in output.iterdir() if p.is_file())
    (output / 'SHA256SUMS').write_text(''.join(
        hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.name + '\n' for p in files))
    print(f'PASS: {VERSION} review artifacts for {revision} -> {output}')
    print('No tag, GitHub release or publication was performed. Artifact review/publication approval remains separate.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-revision', required=True)
    parser.add_argument('--build-dir', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    try:
        prepare(root, args.source_revision, (args.build_dir or root / 'build').resolve())
    except (ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')
