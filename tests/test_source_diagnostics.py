#!/usr/bin/env python3
"""Original-coordinate oracle: expectations come from authored files only."""
import pathlib
import re
import subprocess
import sys
import tempfile

binary = pathlib.Path(sys.argv[1]).resolve()
root = pathlib.Path(__file__).resolve().parents[1]
base = (root / 'tests/fixtures/production/taskboard-rejections/borrowed-alias.jtml').read_text()
work = pathlib.Path(tempfile.mkdtemp(prefix='jtml-source-diagnostics-'))
checks = 0

def reject(entry, expected, component="Candidate"):
    global checks
    output = work / ('out-' + str(checks))
    run = subprocess.run([str(binary), 'build', str(entry), '--target', 'browser', '--out', str(output)],
                         capture_output=True, text=True, timeout=20)
    diagnostic = run.stdout + run.stderr
    (work / ('diagnostic-' + str(checks) + '.log')).write_text(diagnostic)
    assert run.returncode != 0, diagnostic
    assert not any((output / f).exists() for f in ('index.html', 'app.js', 'jtml-runtime.js', 'components/index.js'))
    allowed = {}
    for file, line, column, token in expected:
        allowed.setdefault(str(file), set()).add((line, column))
    for file, line, column, token in expected:
        # Independent check against original authored bytes, never emitted plans.
        authored = file.read_text().splitlines()
        assert authored[line-1].index(token) + 1 == column
        location = str(file) + ':' + str(line) + ':' + str(column)
        assert location in diagnostic, (location, diagnostic)
        positions = [(int(a), int(b)) for a,b in re.findall(re.escape(str(file)) + r':(\d+):(\d+)', diagnostic)]
        assert positions and all(p in allowed[str(file)] for p in positions), (expected, diagnostic)
    if component is not None:
        assert 'component ' + component in diagnostic and 'use ' in diagnostic, diagnostic
    assert 'lowered line' not in diagnostic and 'generated code attributed' not in diagnostic, diagnostic
    checks += 1

# CRLF, Unicode and blank lines alter byte/line offsets without changing semantics.
variant = work / 'variant.jtml'
variant.write_bytes(base.replace('jtml 2\n', 'jtml 2\n\n# é 😀\n', 1).replace('\n', '\r\n').encode())
reject(variant, [(variant, 11, 5, 'alias.inner.score')])

# Selected named import and a re-export preserve the imported declaration origin.
module = work / 'candidate.jtml'
module.write_text(base.replace('make Candidate', 'export make Candidate', 1).split('page\n')[0])
bridge = work / 'bridge.jtml'
bridge.write_text('jtml 2\nexport use Candidate from "./candidate.jtml"\n')
entry = work / 'index.jtml'
entry.write_text('jtml 2\nuse Candidate from "./bridge.jtml"\npage\n  p "Rejected"\n')
reject(entry, [(module, 9, 5, 'alias.inner.score')])

# Identical component and expression spellings in different imported modules
# retain distinct diagnostic locations. Both are rejected before assets.
left = work / 'left.jtml'
right = work / 'right.jtml'
left.write_text(base.split('page\n')[0])
right.write_text('\n\n' + base.split('page\n')[0])
entry2 = work / 'duplicates.jtml'
entry2.write_text('jtml 2\nuse "./left.jtml"\nuse "./right.jtml"\npage\n  p "Rejected"\n')
reject(entry2, [(left, 9, 5, 'alias.inner.score'), (right, 11, 5, 'alias.inner.score')])

# Unsupported indentation is diagnosed during lowering, including imported
# source whose linked line differs from its original line.
tabbed = work / 'tabbed.jtml'
tabbed.write_text('jtml 2\n\nmake Bad\n\ttext "bad"\n')
tab_entry = work / 'tab-entry.jtml'
tab_entry.write_text('jtml 2\nuse "./tabbed.jtml"\npage\n  p "x"\n')
reject(tab_entry, [(tabbed, 4, 1, '\t')], component=None)
scope = root / 'tests/fixtures/production/reference-app/regressions/scope-leak-rejected.jtml'
reject(scope, [(scope, 6, 7, 'let item = input')], component='Child')

repeated = work / 'repeated.jtml'
repeated.write_text(base.replace('    alias.inner.score = 1\n', '    alias.inner.score = 1\n    alias.inner.score = 1\n'))
reject(repeated, [(repeated, 9, 5, 'alias.inner.score'), (repeated, 10, 5, 'alias.inner.score')])

classic = work / 'multiline-classic.jtml'
classic.write_text('define value = (\n  1 + ~\n)\\\\\n')
reject(classic, [(classic, 2, 7, '~')], component=None)
print('PASS', checks, 'adversarial original-source diagnostic cases; evidence', work)
