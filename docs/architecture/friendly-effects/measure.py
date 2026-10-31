"""Measure a rejected cost probe; never edit or build production runtime code."""
import gzip
import json
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parent
runtime = pathlib.Path(sys.argv[1]).read_bytes()
probe = (root / 'rejected-cost-probe.js.txt').read_text()
# Comments are explanatory evidence, not part of the proposed emitted fragment.
code = ('\n' + '\n'.join(line for line in probe.splitlines() if not line.startswith('//')) + '\n').encode()
result = {
    'baseline_runtime_bytes': len(runtime),
    'budget_bytes': 272000,
    'headroom_bytes': 272000 - len(runtime),
    'rejected_probe_bytes': len(code),
    'runtime_with_probe_bytes': len(runtime + code),
    'over_budget_bytes': len(runtime + code) - 272000,
    'baseline_gzip_bytes': len(gzip.compress(runtime, mtime=0)),
    'runtime_with_probe_gzip_bytes': len(gzip.compress(runtime + code, mtime=0)),
    'status': 'rejected cost experiment; integration, write capture, generation and ordering incomplete',
}
# Demonstrated duplication: two identical three-field normalization blocks.
# This experiment is NOT applied to src/ or dist/. Fresh dictionary copies are
# retained at each call; full behavioral verification is still required.
text = runtime.decode()
pattern = r"(?m)^ +emitArity: record\.emitArity.*\n +emitPayloads: record\.emitPayloads.*\n +emitPayloadTypes: record\.emitPayloadTypes.*\n"
blocks = re.findall(pattern, text)
assert len(blocks) == 2
assert [x.strip() for x in blocks[0].splitlines()] == [x.strip() for x in blocks[1].splitlines()]
helper = '      function componentEmitContract(record) {\n        return {\n' + ''.join('          ' + x.strip() + '\n' for x in blocks[0].splitlines()) + '        };\n      }\n'
extracted = re.sub(pattern, '              ...componentEmitContract(record),\n', text)
extracted = extracted.replace('      function scanComponentInstances() {', helper + '      function scanComponentInstances() {', 1).encode()
result['duplicate_normalization_blocks'] = len(blocks)
result['experimental_extraction_runtime_bytes'] = len(extracted)
result['experimental_extraction_saving_bytes'] = len(runtime) - len(extracted)
result['extraction_plus_rejected_probe_bytes'] = len(extracted + code)
result['extraction_plus_rejected_probe_over_budget'] = len(extracted + code) - 272000
print(json.dumps(result, indent=2))
