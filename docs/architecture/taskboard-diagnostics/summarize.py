#!/usr/bin/env python3
"""Aggregate saved compiler diagnostics; never inspect application source."""
import argparse
import collections
import hashlib
import json
from pathlib import Path

root = Path(__file__).parent / 'evidence'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--check', action='store_true', help='check the saved summary without rewriting it')
args = parser.parse_args()
def load(stage):
    return [json.loads(line) for line in (root / (stage + '-diagnostics.jsonl')).read_text().splitlines()]
before, after = load('before'), load('after')
def decision(d):
    return d['severity'], d['code'], d['message']
sequence = json.dumps([decision(d) for d in before], separators=(',', ':')).encode()
history = json.loads((root / 'historical-runs.json').read_text())
assert hashlib.sha256(sequence).hexdigest() == history['baselineObserver']['decisionSequenceSha256']
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
if args.check:
    assert summary == json.loads((root / 'summary.json').read_text()), 'saved summary differs'
else:
    (root / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(summary, indent=2))
