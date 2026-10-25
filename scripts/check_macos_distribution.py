#!/usr/bin/env python3
"""Audit actual CLI/C API Mach-O files. Static checks do not prove older-host execution."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

MINIMUM = '14.0'
# Both are imported by the failed macOS 15 artifact and unavailable on 14.4.1.
FORBIDDEN_IMPORTS = ('__ZNSt13exception_ptr31__from_native_exception_pointerEPv',
                     '___cxa_init_primary_exception')


def validate_load_commands(text):
    builds = re.findall(r'\bcmd LC_BUILD_VERSION\b(.*?)(?=Load command|\Z)', text, re.S)
    legacy = re.findall(r'\bcmd LC_VERSION_MIN_MACOSX\b(.*?)(?=Load command|\Z)', text, re.S)
    if len(builds) + len(legacy) != 1:
        raise ValueError('Expected one macOS minimum-OS load command')
    block = (builds or legacy)[0]
    if builds and not re.search(r'\bplatform (?:1|MACOS)\b', block):
        raise ValueError('Expected macOS platform')
    minimum = re.search(r'\b(?:minos|version) (\d+(?:\.\d+)+)', block)
    sdk = re.search(r'\bsdk (\d+(?:\.\d+)+)', block)
    if not minimum or minimum[1] not in ('14.0', '14.0.0') or not sdk:
        raise ValueError('CLI and C API must declare minimum macOS 14.0; got ' + block.strip())
    return dict(minimumOS=minimum[1], sdk=sdk[1])


def validate_imports(text):
    for symbol in FORBIDDEN_IMPORTS:
        if symbol in text.split():
            raise ValueError('macOS 14-incompatible imported symbol: ' + symbol)


def validate_libraries(text):
    libraries = [line.strip().split(' (', 1)[0] for line in text.splitlines()[1:] if line.strip()]
    if not libraries or '/usr/lib/libc++.1.dylib' not in libraries:
        raise ValueError('Missing expected system libc++ dependency')
    for library in libraries:
        if not (library.startswith(('/usr/lib/', '/System/Library/')) or
                library == '@rpath/libjtml.dylib'):
            raise ValueError('Unpackaged/non-system dynamic dependency: ' + library)
    return libraries


def audit(prefix):
    binaries = [prefix / 'bin/jtml', prefix / 'lib/libjtml.dylib']
    reports = []
    for binary in binaries:
        def run(*args):
            return subprocess.check_output([*args, str(binary)], text=True)
        if run('lipo', '-archs').strip() != 'arm64':
            raise ValueError('RC macOS artifacts must be arm64: ' + str(binary))
        load = run('otool', '-l')
        symbols = run('nm', '-u')
        libraries = validate_libraries(run('otool', '-L'))
        validate_imports(symbols)
        metadata = validate_load_commands(load)
        reports.append(dict(file=binary.relative_to(prefix).as_posix(),
                            sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                            architecture='arm64', libraries=libraries,
                            undefinedSymbols=sorted(symbols.split()), **metadata))
    return dict(schemaVersion=1, intendedMinimumOS=MINIMUM, binaries=reports,
                limitation='Static audit; macOS 15 execution does not prove macOS 14 compatibility')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('prefix', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        report = audit(args.prefix.resolve())
        if args.output:
            args.output.write_text(json.dumps(report, sort_keys=True, indent=2) + '\n')
        print('PASS: CLI and C API arm64/minimum macOS 14.0/system dylibs/known incompatible imports')
    except (ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')
