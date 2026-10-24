#!/usr/bin/env python3
"""Aggregate saved compiler diagnostics; never inspect application source."""
import collections
import csv
import json
import re
from pathlib import Path

root = Path(__file__).parent / 'evidence'
before = json.loads((root / 'before-context.json').read_text())['diagnostics']
after = json.loads((root / 'after-lint.json').read_text())['diagnostics']
raw = json.loads((root / 'before-lint.json').read_text())['diagnostics']
def decision(d):
    return d['severity'], d['code'], d['message']
assert list(map(decision, before)) == list(map(decision, raw))
def key(d):
    return (*decision(d), d.get('scope', ''))
remaining = collections.Counter(map(key, after))
removed = []
for diagnostic in before:
    if remaining[key(diagnostic)]:
        remaining[key(diagnostic)] -= 1
    else:
        removed.append(diagnostic)
assert not sum(remaining.values()), 'candidate introduced diagnostics'
assert len(removed) == 302
removed_keys = set(map(key, removed))
def category(d):
    if key(d) in removed_keys:
        return 'erased-local-declaration'
    if d['code'] == 'JTML_ERROR':
        return 'emit-callback-binding'
    if d['code'] == 'JTML_ARITY':
        return 'keyboard-implicit-event-argument'
    if d['code'] == 'JTML_TEMPLATE_WHILE':
        return 'action-versus-template-context'
    if d['code'] == 'JTML_UI_SURFACE_UNLABELED':
        return 'unlabeled-card-advisory'
    if d['message'].startswith('assignment to undefined'):
        return 'generated-input-setter-owner'
    assert d['code'] == 'JTML_UNDEFINED_NAME'
    return 'emit-argument-lowering'

def counts(items, fields):
    return [dict(zip(fields, values), count=count)
            for values, count in sorted(collections.Counter(
                tuple(d.get(field, '') for field in fields) for d in items).items())]
summary = {
    'baseline': '315e6f280169330f0f6a7528484f3f78b8d5a1f9',
    'beforeByCode': counts(before, ['severity', 'code']),
    'afterByCode': counts(after, ['severity', 'code']),
    'removedByComponent': counts(removed, ['component']),
    'beforeCategories': dict(sorted(collections.Counter(map(category, before)).items())),
    'remainingCategories': dict(sorted(collections.Counter(map(category, after)).items())),
    'unlocatedBefore': sum(not d['line'] for d in before),
    'unlocatedAfter': sum(not d['line'] for d in after),
    'newDiagnostics': 0,
}
(root / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
for stage, items in [('before', before), ('after', after)]:
    rows = []
    for d in items:
        match = re.search(r"'([^']+)'", d['message'])
        lowered = match.group(1) if match else ''
        variable = re.sub(r'^(?:__[A-Za-z][A-Za-z0-9_]*?_\d+_)+', '', lowered)
        file = d.get('file', '')
        if '/tests/' in file:
            file = 'tests/' + file.split('/tests/', 1)[1]
        rows.append((d['severity'], d['code'], category(d), variable, lowered,
                     d.get('component', ''), d.get('scope', ''), file,
                     d['line'], d['column'], d.get('sourceExact', False)))
    with (root / (stage + '-inventory.tsv')).open('w') as stream:
        writer = csv.writer(stream, delimiter='\t', lineterminator='\n')
        writer.writerow(['severity', 'code', 'category', 'variable', 'loweredVariable',
                         'component', 'scope', 'file', 'line', 'column', 'sourceExact', 'count'])
        for row, count in sorted(collections.Counter(rows).items()):
            writer.writerow([*row, count])
print(json.dumps(summary, indent=2))
