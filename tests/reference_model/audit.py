"""Fixture/source integrity checks; not a JTML parser or semantic oracle."""
import hashlib
import json
from pathlib import Path

from model import fields, run, ModelError

ROOT = Path(__file__).resolve().parents[2]
SELECTED = ("V01", "V03", "V04", "V05")


def load_json(path):
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ModelError("E_SCHEMA", f"duplicate JSON key: {key}")
            result[key] = value
        return result
    return json.loads(Path(path).read_text(encoding="utf-8"), object_pairs_hook=pairs,
                      parse_constant=lambda value: (_ for _ in ()).throw(
                          ModelError("E_SCHEMA", f"non-JSON constant: {value}")))


def digest(value):
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def program_digest(program):
    return digest(json.dumps(program, sort_keys=True, separators=(",", ":"), ensure_ascii=False))


def pointer(program, path):
    node = program
    for part in path.lstrip("/").split("/"):
        node = node[int(part)] if isinstance(node, list) else node[part]
    return node


def load_fixture(vector, root=ROOT):
    if vector not in SELECTED:
        raise ModelError("UNSUPPORTED", "only four audited fixtures are selected")
    fixture = load_json(root / "tests/reference_model/fixtures" / (vector + ".json"))
    fields(fixture, {"schema_version", "vector", "source", "program", "program_sha256", "audit", "steps"})
    if fixture["schema_version"] != 1 or fixture["vector"] != vector:
        raise ModelError("E_AUDIT", "fixture identity/version mismatch")
    source = fixture["source"]
    fields(source, {"catalog", "file", "text", "sha256"})
    if source["catalog"] != "docs/spec/reactive-core-v1/vectors.json":
        raise ModelError("E_AUDIT", "unexpected source catalog")
    catalog = load_json(root / source["catalog"])
    original = next(v for v in catalog["vectors"] if v["id"] == vector)
    if original["files"] != {source["file"]: source["text"]} or digest(source["text"]) != source["sha256"]:
        raise ModelError("E_AUDIT", "original source bytes/digest mismatch")
    if program_digest(fixture["program"]) != fixture["program_sha256"]:
        raise ModelError("E_AUDIT", "reviewed AST digest mismatch")
    expected_steps = [{"action": entry["action"], "args": entry["args"]} for entry in original["steps"]]
    if fixture["steps"] != expected_steps:
        raise ModelError("E_AUDIT", "external entry sequence mismatch")
    lines = source["text"].splitlines()
    if len(fixture["audit"]) != len(lines):
        raise ModelError("E_AUDIT", "audit must account for every source line")
    covered = set()
    for number, (line, review) in enumerate(zip(lines, fixture["audit"]), 1):
        fields(review, {"line", "text", "role", "model_paths"})
        if review["line"] != number or review["text"] != line:
            raise ModelError("E_AUDIT", "source crosswalk mismatch")
        if review["role"] not in {"dialect", "declaration", "assignment", "action", "virtual-text", "static-envelope", "external-entry"}:
            raise ModelError("E_AUDIT", "unknown review role")
        for path in review["model_paths"]:
            node = pointer(fixture["program"], path)
            covered.add(path)
            if review["role"] in {"declaration", "assignment", "action", "virtual-text"}:
                actual = node["source"]
                if actual != {"file": source["file"], "line": number,
                              "column": len(line) - len(line.lstrip()) + 1}:
                    raise ModelError("E_AUDIT", "AST source position mismatch")
    program = fixture["program"]
    required = {f"/declarations/{i}" for i in range(len(program["declarations"]))}
    required |= {f"/actions/{i}" for i in range(len(program["actions"]))}
    required |= {f"/actions/{i}/body/{j}" for i, a in enumerate(program["actions"]) for j in range(len(a["body"]))}
    required |= {f"/observers/{i}" for i in range(len(program["observers"]))}
    if not required <= covered:
        raise ModelError("E_AUDIT", "unaudited executable node")
    run(program, fixture["steps"])  # Closed model schema + semantic validation, no expected-output input.
    return fixture


if __name__ == "__main__":
    for selected in SELECTED:
        fixture = load_fixture(selected)
        print(f"{selected}: schema/source/AST crosswalk PASS ({fixture['source']['sha256']})")
