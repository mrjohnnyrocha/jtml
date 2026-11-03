import copy
from itertools import permutations
import json
from pathlib import Path
import tempfile
import unittest

from audit import ROOT, SELECTED, load_fixture
from compare import compare, same
from model import ModelError, PendingPolicy, Unsupported, run


def number(value):
    return {"literal": {"kind": "number", "value": str(value)}}


def read(key):
    return {"read": key}


def binary(op, left, right):
    return {"binary": op, "left": left, "right": right}


def synthetic_declaration(key, kind, expression, line=2):
    return {"id": key, "name": key, "kind": kind, "expression": expression,
            "source": {"file": "synthetic.jtml", "line": line, "column": 1}}


class ReferenceModelTests(unittest.TestCase):
    def fixture(self, vector):
        return copy.deepcopy(load_fixture(vector))

    def test_four_authored_oracles(self):
        report = compare()
        self.assertEqual([r["vector"] for r in report["results"]], list(SELECTED))
        for row in report["results"]:
            with self.subTest(vector=row["vector"]):
                self.assertEqual(row["model"], "PASS", row)
                self.assertEqual([c["sequence"] for c in row["model_commits"]],
                                 list(range(len(row["expected"]))))

    def test_required_fresh_values(self):
        cases = {v: self.fixture(v) for v in SELECTED}
        commits = {v: run(f["program"], f["steps"]) for v, f in cases.items()}
        self.assertEqual([dict(c.state)["main::x"] for c in commits["V01"]], [1, 2, 3])
        self.assertEqual(dict(commits["V03"][1].state)["main::saved"], 2)
        self.assertEqual(dict(commits["V04"][1].derived), {"main::a": 6, "main::z": 3})
        self.assertEqual(dict(commits["V05"][1].derived),
                         {"main::zleft": 3, "main::zright": 4, "main::atotal": 7})

    def test_independent_mutable_declarations_can_reorder(self):
        f = self.fixture("V01")
        f["program"]["declarations"].extend([
            synthetic_declaration("unrelated-a", "let", number(7)),
            synthetic_declaration("unrelated-b", "let", number(13))])
        expected = run(f["program"], f["steps"])
        for declarations in permutations(f["program"]["declarations"]):
            program = copy.deepcopy(f["program"])
            program["declarations"] = list(declarations)
            self.assertEqual(run(program, f["steps"]), expected)

    def test_derived_declarations_can_reorder(self):
        f = self.fixture("V05")
        declarations = f["program"]["declarations"]
        expected = run(f["program"], f["steps"])
        for derived in permutations(declarations[1:]):
            program = copy.deepcopy(f["program"])
            program["declarations"] = list(derived) + declarations[:1]
            self.assertEqual(run(program, f["steps"]), expected)

    def test_arbitrary_chains_and_layered_diamonds(self):
        for diamond in (False, True):
            for depth in (1, 3, 6):
                with self.subTest(diamond=diamond, depth=depth):
                    f = self.fixture("V01")
                    program = f["program"]
                    previous = "main::x"
                    for i in range(depth):
                        if diamond:
                            left, right = f"left-{i}", f"right-{i}"
                            program["declarations"].extend([
                                synthetic_declaration(left, "get", binary("+", read(previous), number(1))),
                                synthetic_declaration(right, "get", binary("*", read(previous), number(2)))])
                            expr = binary("+", read(left), read(right))
                        else:
                            expr = binary("+", binary("*", read(previous), number(2)), number(1))
                        previous = f"total-{i}"
                        program["declarations"].append(synthetic_declaration(previous, "get", expr))
                    # Independent closed-form arithmetic, not evaluation of a production plan.
                    scale = 3 ** depth if diamond else 2 ** depth
                    offset = (scale - 1) // 2 if diamond else scale - 1
                    commits = run(program, f["steps"])
                    for x, commit in zip((1, 2, 3), commits):
                        self.assertEqual(dict(commit.derived)[previous], scale * x + offset)
                    program["declarations"].reverse()
                    self.assertEqual(run(program, f["steps"]), commits)

    def test_cycle_is_rejected_before_initial_commit(self):
        f = self.fixture("V04")
        f["program"]["declarations"][1]["expression"] = read("main::a")
        with self.assertRaises(ModelError) as caught:
            run(f["program"], [])
        self.assertEqual(caught.exception.code, "E_DERIVED_CYCLE")
        self.assertEqual(caught.exception.source["file"], "index.jtml")

    def test_unresolved_binding_in_every_consumer_is_rejected(self):
        for consumer in ("initializer", "get", "action", "text"):
            with self.subTest(consumer=consumer):
                f = self.fixture("V03")
                if consumer == "initializer":
                    f["program"]["declarations"][0]["expression"] = read("missing")
                elif consumer == "get":
                    f["program"]["declarations"][2]["expression"] = read("missing")
                elif consumer == "action":
                    f["program"]["actions"][0]["body"][0]["expression"] = read("missing")
                else:
                    f["program"]["observers"][0]["segments"] = [read("missing")]
                with self.assertRaises(ModelError) as caught:
                    run(f["program"], [])
                self.assertEqual(caught.exception.code, "E_UNRESOLVED")

    def test_dependent_initializers_retain_source_order(self):
        f = self.fixture("V01")
        f["program"]["declarations"].append(synthetic_declaration("copy", "let", read("main::x")))
        self.assertEqual(dict(run(f["program"], [])[0].state)["copy"], 1)
        f["program"]["declarations"].reverse()
        with self.assertRaises(ModelError) as caught:
            run(f["program"], [])
        self.assertEqual(caught.exception.code, "E_INIT_FORWARD")

    def test_resolved_id_alpha_renaming_not_name_lookup(self):
        f = self.fixture("V03")
        program = f["program"]
        renamed = {d["id"]: f"opaque-{i}" for i, d in enumerate(program["declarations"])}
        def rename(node):
            if isinstance(node, dict):
                return {k: renamed.get(v, v) if k in {"id", "read", "assign"} and isinstance(v, str)
                        else rename(v) for k, v in node.items()}
            if isinstance(node, list):
                return [rename(v) for v in node]
            return node
        changed = run(rename(program), f["steps"])[1].record()
        self.assertEqual(changed["state"][renamed["main::saved"]], 2)
        self.assertEqual(changed["text"], {".out": ["2|2"]})

    def test_repeated_actions_are_deterministic_and_input_is_unchanged(self):
        f = self.fixture("V01")
        before = copy.deepcopy(f)
        entries = [{"action": "main::step", "args": []}] * 100
        first = run(f["program"], entries)
        self.assertEqual(first, run(f["program"], entries))
        self.assertEqual(len(first), 101)
        self.assertEqual(dict(first[-1].state)["main::x"], 101)
        self.assertEqual(f, before)

    def test_snapshots_cannot_be_changed_through_output_views(self):
        f = self.fixture("V01")
        commits = run(f["program"], f["steps"])
        record = commits[0].record()
        record["state"]["main::x"] = 999
        self.assertEqual(dict(commits[0].state)["main::x"], 1)
        with self.assertRaises(TypeError):
            commits[0].state[0] = ("main::x", 999)

    def test_scalar_kinds_remain_distinct(self):
        f = self.fixture("V01")
        f["program"]["declarations"].extend([
            synthetic_declaration("flag", "let", {"literal": {"kind": "boolean", "value": True}}),
            synthetic_declaration("label", "let", {"literal": {"kind": "string", "value": "hello"}})])
        f["program"]["observers"][0]["segments"] = [read("flag"), read("label")]
        result = run(f["program"], [])[0].record()
        self.assertIs(type(result["state"]["flag"]), bool)
        self.assertEqual(result["text"], {".out": ["truehello"]})
        self.assertFalse(same(True, 1))
        self.assertFalse(same(-0.0, 0.0))

    def test_nonfinite_results_abstain_without_selecting_a6c(self):
        f = self.fixture("V01")
        f["program"]["actions"][0]["body"][0]["expression"] = binary("*", number("1e308"), number("1e308"))
        with self.assertRaises(PendingPolicy) as caught:
            run(f["program"], f["steps"])
        self.assertEqual(caught.exception.code, "PENDING_POLICY")

    def test_boolean_is_not_a_python_number_operand(self):
        f = self.fixture("V01")
        f["program"]["actions"][0]["body"][0]["expression"] = binary("*", {"literal": {"kind": "boolean", "value": True}}, number(2))
        with self.assertRaises(PendingPolicy):
            run(f["program"], f["steps"])

    def test_unsupported_features_and_oracle_fields_fail_closed(self):
        for extra in ("imports", "effects", "components", "expected", "vector"):
            f = self.fixture("V01")
            f["program"][extra] = []
            with self.subTest(extra=extra), self.assertRaises(Unsupported):
                run(f["program"], [])
        f = self.fixture("V01")
        f["program"]["actions"][0]["body"][0]["expression"]["binary"] = "/"
        with self.assertRaises(Unsupported):
            run(f["program"], [])

    def test_malformed_schema_rejects_instead_of_defaulting(self):
        f = self.fixture("V01")
        del f["program"]["observers"]
        with self.assertRaises(ModelError) as caught:
            run(f["program"], [])
        self.assertEqual(caught.exception.code, "E_SCHEMA")

    def test_source_and_ast_digest_tampering_are_detected(self):
        fixture = self.fixture("V01")
        for tamper in ("source", "program"):
            with self.subTest(tamper=tamper), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                path = root / "tests/reference_model/fixtures/V01.json"
                path.parent.mkdir(parents=True)
                catalog = root / fixture["source"]["catalog"]
                catalog.parent.mkdir(parents=True)
                catalog.write_bytes((ROOT / fixture["source"]["catalog"]).read_bytes())
                broken = copy.deepcopy(fixture)
                if tamper == "source":
                    broken["source"]["text"] += "\n"
                else:
                    broken["program"]["declarations"][0]["expression"] = number(99)
                path.write_text(json.dumps(broken))
                with self.assertRaises(ModelError) as caught:
                    load_fixture("V01", root)
                self.assertEqual(caught.exception.code, "E_AUDIT")

    def test_preserved_browser_failures_are_not_model_passes(self):
        rows = {r["vector"]: r for r in compare()["results"]}
        self.assertEqual(rows["V01"]["browser_snapshot"], "PASS")
        for vector in ("V03", "V04", "V05"):
            self.assertEqual(rows[vector]["browser_snapshot"], "FAIL")
            self.assertEqual(rows[vector]["browser_phase_trace"], "UNVERIFIED")
            self.assertEqual(rows[vector]["full_core_conformance"], "UNVERIFIED")


if __name__ == "__main__":
    unittest.main()
