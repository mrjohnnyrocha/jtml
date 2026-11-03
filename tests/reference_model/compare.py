"""Compare authored expectations, independent model results, and preserved observations."""
import argparse
import json
from math import copysign
from pathlib import Path

from audit import ROOT, SELECTED, digest, load_fixture, load_json
from model import run


def same(left, right):
    if isinstance(left, bool) or isinstance(right, bool):
        return type(left) is bool and type(right) is bool and left == right
    if isinstance(left, (int, float)) and isinstance(right, (int, float)):
        return left == right and (left != 0 or copysign(1, left) == copysign(1, right))
    if type(left) is not type(right):
        return False
    if isinstance(left, dict):
        return left.keys() == right.keys() and all(same(left[k], right[k]) for k in left)
    if isinstance(left, list):
        return len(left) == len(right) and all(same(a, b) for a, b in zip(left, right))
    return left == right


def compare(root=ROOT):
    catalog_path = root / "docs/spec/reactive-core-v1/vectors.json"
    browser_path = root / "docs/spec/reactive-core-v1/baseline-observations.json"
    catalog, browser = load_json(catalog_path), load_json(browser_path)
    rows = []
    for selected in SELECTED:
        fixture = load_fixture(selected, root)
        # The evaluator gets only the AST and action inputs. Oracles are read here afterwards.
        commits = [c.record() for c in run(fixture["program"], fixture["steps"])]
        vector = next(v for v in catalog["vectors"] if v["id"] == selected)
        expected = [vector["expected_initial"]] + [s["expected"] for s in vector["steps"]]
        if any(e["outcome"] != "commit" or e["notifications"] or e["effects"] or e["show"] for e in expected):
            raise ValueError("oracle requires features outside this four-vector success slice")
        oracle = [{k: e[k] for k in ("state", "derived", "dom")} for e in expected]
        actual = [{"state": c["state"], "derived": c["derived"], "dom": c["text"]} for c in commits]
        observed = next((v for v in browser["results"] if v["id"] == selected), None)
        snapshots, missing = [], []
        if observed is not None and observed["buildExit"] == 0:
            for index, snap in enumerate(observed["observations"]):
                values = snap["rawRuntimeStorage"]
                fields = {"state": {}, "derived": {}, "dom": {".out": snap["dom"]}}
                for declaration in fixture["program"]["declarations"]:
                    name = declaration["name"]
                    if name not in values:
                        missing.append({"snapshot": index, "name": name})
                    else:
                        section = "state" if declaration["kind"] == "let" else "derived"
                        fields[section][declaration["id"]] = values[name]
                snapshots.append(fields)
        browser_status = "UNVERIFIED" if missing or len(snapshots) != len(oracle) else "PASS" if same(oracle, snapshots) else "FAIL"
        rows.append({"vector": selected, "model": "PASS" if same(oracle, actual) else "FAIL",
                     "browser_snapshot": browser_status,
                     "browser_phase_trace": "UNVERIFIED", "full_core_conformance": "UNVERIFIED",
                     "expected": oracle, "model_commits": commits,
                     "browser_snapshots": snapshots, "missing_browser_fields": missing})
    return {"schema_version": 1, "model_profile": "s1-minimal-success-v1",
            "model_checkout_baseline": "27d05e9497911f89f3d463dd0fb68ea67fe65b33",
            "recorded_browser_baseline": browser["baseline"],
            "browser_evidence_sha256": digest(browser_path.read_text(encoding="utf-8")),
            "scope": "Four success vectors only. Browser data is historical PR #77 evidence; no browser rerun. Snapshot PASS is not phase/lifetime/DOM atomicity conformance.",
            "results": rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = compare()
    encoded = json.dumps(report, indent=2, sort_keys=True, ensure_ascii=False, allow_nan=False) + "\n"
    if args.output:
        args.output.write_text(encoded, encoding="utf-8")
    for row in report["results"]:
        print(f"{row['vector']}: MODEL {row['model']} / BROWSER SNAPSHOT {row['browser_snapshot']} / PHASE TRACE UNVERIFIED")
    # Known browser discrepancies stay FAIL, not a green conformance command.
    return 1 if any(r["model"] == "FAIL" or r["browser_snapshot"] == "FAIL" for r in report["results"]) else 0


if __name__ == "__main__":
    raise SystemExit(main())
