#!/usr/bin/env python3
"""Measure actual entrypoints; retain failures as data, never call them functional."""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, default=ROOT / 'build/jtml')
    parser.add_argument('--out', type=Path, default=ROOT / 'dist/application-inventory')
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    sources = [ROOT / p for p in ('apps/releasedesk/index.jtml', 'demos/index.jtml',
               'demos/example.jtml', 'tests/fixtures/production/taskboard-core/index.jtml')]
    sources += sorted((ROOT / 'demos/opspulse').rglob('*.jtml'))
    sources += sorted((ROOT / 'examples').rglob('*.jtml'))
    sources += sorted((ROOT / 'examples').rglob('*.jtl'))
    sources += sorted((ROOT / 'studio/samples').glob('*.jtml'))
    report = {'compiler': str(args.compiler.resolve()), 'results': []}
    report['version'] = subprocess.check_output([str(args.compiler.resolve()), '--version'], text=True).strip()
    for source in sources:
        relative = source.relative_to(ROOT).as_posix()
        row = {'source': relative, 'commands': []}
        for verb in ('check', 'lint', 'build'):
            command = [str(args.compiler.resolve()), verb, str(source)]
            if verb == 'build':
                command += ['--target', 'browser', '--out', str(args.out / 'builds' / relative)]
            try:
                result = subprocess.run(command, capture_output=True, text=True, timeout=30, cwd=ROOT)
                row['commands'].append({'command': command, 'exit': result.returncode,
                                        'stdout': result.stdout, 'stderr': result.stderr})
            except subprocess.TimeoutExpired:
                row['commands'].append({'command': command, 'exit': None, 'error': '30-second timeout'})
        report['results'].append(row)
        (args.out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
        print(relative, [c['exit'] for c in row['commands']], flush=True)
    print('Inventory recorded; zero runner exit is not an application acceptance result.')


if __name__ == '__main__':
    main()
