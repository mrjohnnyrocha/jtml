"""JSON transport from the reviewed fixtures to the independent evaluator."""
import json
import sys

from model import fields, run_trace

document = json.load(sys.stdin)
fields(document, {"policy", "programs"})
print(json.dumps([run_trace(program, steps, document["policy"])
                  for program, steps in document["programs"]], allow_nan=False))
