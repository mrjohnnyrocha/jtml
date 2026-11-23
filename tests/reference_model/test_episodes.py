"""Small hand-authored checks for the experimental oracle itself."""
import copy
import unittest

from audit import load_fixture
from model import EXPERIMENT, P3, run_trace
from test_model import binary, number, read, synthetic_declaration

LOCATION = {"file": "synthetic.jtml", "line": 2, "column": 1}


def assign(target, expr):
    return {"assign": target, "expression": expr, "source": LOCATION}


def show(expr):
    return {"show": expr, "source": LOCATION}


def effect(name, watch, line, *body):
    return {"id": name, "watch": watch, "body": list(body), "source": {**LOCATION, "line": line}}


def scenario():
    fixture = copy.deepcopy(load_fixture("V01"))
    program = {**fixture["program"], "profile": EXPERIMENT, "effects": []}
    return program, fixture["steps"]


class EpisodeTests(unittest.TestCase):
    def test_failure_policy_has_no_default(self):
        program, steps = scenario()
        self.assertEqual(run_trace(program, steps), {"validation": "PENDING_POLICY"})

    def test_same_value_and_parameter_shadowing(self):
        program, _ = scenario()
        action = program["actions"][0]
        action["params"] = ["step::x"]
        program["declarations"].append(synthetic_declaration("twice", "get", binary("*", read("main::x"), number("2"))))
        action["body"] = [assign("step::x", number("99")), show(read("twice")),
                          assign("main::x", read("main::x"))]
        program["effects"] = [effect("E1", "main::x", 8, show(read("main::x")))]
        step = {"action": "main::step", "args": [number("3")]}
        result = run_trace(program, [step, step], P3)
        self.assertEqual([step["commit"]["sequence"] for step in result["steps"]], [1, 2])
        for step in result["steps"]:
            self.assertEqual(step["commit"]["state"]["main::x"], {"kind": "number", "value": 1})
            self.assertEqual([event["kind"] for event in step["events"]],
                             ["show", "notify", "effect-enter", "show", "effect-exit"])
            self.assertEqual([event["text"] for event in step["events"] if event["kind"] == "show"], ["2", "1"])

    def test_potential_feedback_in_inactive_branch_rejects(self):
        program, steps = scenario()
        conditional = {"if": {"literal": {"kind": "boolean", "value": False}},
                       "then": [assign("main::x", number("9"))], "else": [], "source": LOCATION}
        program["effects"] = [effect("E1", "main::x", 9, conditional)]
        result = run_trace(program, steps, P3)
        self.assertEqual(result["code"], "E_EFFECT_CYCLE")
        self.assertNotIn("initial", result)

    def test_later_effect_fault_keeps_prefix_only_and_aborts_dispatch(self):
        program, steps = scenario()
        program["declarations"].extend(synthetic_declaration(key, "let", number("0")) for key in ("y", "marker"))
        program["effects"] = [
            effect("E2", "y", 4, show(read("y")), assign("marker", binary("/", number("1"), number("0")))),
            effect("E3", "y", 6, show(number("100"))),
            effect("E1", "main::x", 8, assign("y", number("3"))),
        ]
        result = run_trace(program, steps, P3)
        attempted, denied = result["steps"]
        self.assertEqual(attempted["outcome"], "episode-error")
        self.assertEqual([e["effect"] for e in attempted["events"] if e["kind"] == "effect-enter"], ["E1", "E2"])
        self.assertEqual(attempted["events"][-1], {"kind": "effect-exit", "effect": "E2", "outcome": "error"})
        self.assertEqual(attempted["failedPartial"]["y"], {"kind": "number", "value": 3})
        self.assertEqual(attempted["failedPartial"]["marker"], {"kind": "number", "value": 0})
        self.assertEqual(denied, {"outcome": "dispatch-rejected", "events": [], "lastCommit": result["initial"]})
        self.assertNotIn("commit", attempted)

    def test_scalar_kind_signed_zero_and_lazy_branch(self):
        program, steps = scenario()
        program["declarations"].append(synthetic_declaration("zero", "let", number("-0")))
        expression = binary("||", {"literal": {"kind": "boolean", "value": True}},
                            binary(">", binary("/", number("1"), number("0")), number("0")))
        program["actions"][0]["body"].append(show(expression))
        result = run_trace(program, steps[:1], P3)
        self.assertEqual(result["initial"]["state"]["zero"], {"kind": "number", "value": "-0"})
        self.assertEqual(result["steps"][0]["events"][0]["scalar"], {"kind": "boolean", "value": True})
        program["actions"][0]["body"].append(assign("zero", expression))
        self.assertEqual(run_trace(program, steps, P3)["code"], "E_UNSUPPORTED_EXPR")
