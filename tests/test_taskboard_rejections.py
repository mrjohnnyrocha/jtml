#!/usr/bin/env python3
"""N04: fail-closed checks plus independent original-location acceptance audit.

A provenance gap is BLOCKED, never a passing N04. Functional rejection failures
are fatal. Source-span repair is deliberately outside this PR's scope.
"""
import json
import pathlib
import re
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[1]
binary = pathlib.Path(sys.argv[1]).resolve()
output = pathlib.Path(sys.argv[2]).resolve()
output.mkdir(parents=True, exist_ok=True)
fixtures = root / 'tests/fixtures/production/taskboard-rejections'
cases = [
    ('borrowed-alias', 'alias.inner.score', 9, 5, 'borrowed'),
    ('borrowed-loop', 'alias.score', 10, 9, 'borrowed'),
    ('rhs-call', 'result = mystery()', 7, 5, 'assignment'),
    ('target-call', 'items[mystery()].score', 7, 5, 'assignment'),
    ('collision', 'let item = incoming', 8, 7, 'nested declaration'),
]
results = []
for name, token, line, column, remediation in cases:
    source = fixtures / (name + '.jtml')
    lines = source.read_text().splitlines()
    assert lines[line - 1].index(token) + 1 == column, (name, 'oracle source coordinate drift')
    destination = output / name
    assert not destination.exists(), 'use a fresh output directory to detect partial emission'
    run = subprocess.run([str(binary), 'build', str(source), '--target', 'browser', '--out', str(destination)],
                         capture_output=True, text=True, timeout=15)
    diagnostic = run.stdout + run.stderr
    assert run.returncode != 0, (name, 'accepted unsupported mutation')
    assert not any((destination / f).exists() for f in ('index.html', 'app.js', 'components/index.js', 'jtml-runtime.js')), (name, 'runnable output on rejection')
    assert str(source) in diagnostic and 'Candidate' in diagnostic and remediation in diagnostic, (name, diagnostic)
    assert any(word in diagnostic for word in ('use ', 'assign ', 'supported', 'simplify')), diagnostic
    exact = bool(re.search(re.escape(str(source)) + ':' + str(line) + ':' + str(column) + r'\b', diagnostic))
    linked = [(int(a), int(b)) for a, b in re.findall(r'at line (\d+):(\d+)', diagnostic)]
    exact = exact and all(position == (line, column) for position in linked)
    results.append(dict(case=name, rejection='PASS', provenance='PASS' if exact else 'BLOCKED', linkedLocations=linked,
                        expected=dict(file=str(source), line=line, column=column, component='Candidate'), diagnostic=diagnostic.strip()))
# Preserve and exercise the exact previously approved scope leak, unmodified.
source = root / 'tests/fixtures/production/reference-app/regressions/scope-leak-rejected.jtml'
destination = output / 'original-scope-leak'
run = subprocess.run([str(binary), 'build', str(source), '--target', 'browser', '--out', str(destination)], capture_output=True, text=True, timeout=15)
assert run.returncode != 0 and 'nested declaration `item`' in run.stderr, run
assert not any((destination / f).exists() for f in ('index.html','app.js','components/index.js','jtml-runtime.js'))
report = dict(N04='PASS' if all(r['provenance']=='PASS' for r in results) else 'BLOCKED',
              failClosed='PASS', originalScopeLeak='PASS', cases=results)
(output / 'n04-results.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))

if '--require-provenance' in sys.argv[3:] and report['N04'] != 'PASS':
    sys.exit(2)
