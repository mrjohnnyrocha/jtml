"""Independent, deliberately small scalar model; no source parser or file IO."""
from dataclasses import dataclass
from math import isfinite
import re
from types import MappingProxyType

PROFILE = "s1-minimal-success-v1"


class ModelError(Exception):
    def __init__(self, code, message, source=None):
        self.code, self.message, self.source = code, message, source
        super().__init__(f"{code}: {message}")


class Unsupported(ModelError):
    pass


class PendingPolicy(ModelError):
    """Abstention, not a rollback, recoverability or instance-fault decision."""


def fields(value, required):
    if not isinstance(value, dict):
        raise ModelError("E_SCHEMA", "expected object")
    extra = set(value) - set(required)
    if extra:
        raise Unsupported("UNSUPPORTED", f"unsupported fields: {sorted(extra)}")
    if set(required) - set(value):
        raise ModelError("E_SCHEMA", f"required fields: {sorted(required)}")


def array(value):
    if not isinstance(value, list):
        raise ModelError("E_SCHEMA", "expected array")
    return value


def identifier(value):
    if not isinstance(value, str) or not value:
        raise ModelError("E_SCHEMA", "expected nonempty identity/name")
    return value


def origin(value):
    fields(value, {"file", "line", "column"})
    identifier(value["file"])
    for key in ("line", "column"):
        if type(value[key]) is not int or value[key] < 1:
            raise ModelError("E_SCHEMA", "source positions must be positive integers")


def literal(node):
    fields(node, {"kind", "value"})
    kind, value = node["kind"], node["value"]
    if kind == "boolean" and type(value) is bool:
        return value
    if kind == "string" and isinstance(value, str):
        if not any(0xD800 <= ord(ch) <= 0xDFFF for ch in value):
            return value
    if kind == "number" and isinstance(value, str):
        if re.fullmatch(r"-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?", value):
            number = float(value)
            if isfinite(number):
                return number
    raise ModelError("E_SCALAR_LITERAL", "invalid S1 scalar literal")


def references(expr):
    """Validate the closed expression schema while collecting resolved reads."""
    if not isinstance(expr, dict):
        raise ModelError("E_SCHEMA", "expression must be an object")
    if set(expr) == {"literal"}:
        literal(expr["literal"])
        return set()
    if set(expr) == {"read"}:
        return {identifier(expr["read"])}
    if set(expr) == {"binary", "left", "right"}:
        if expr["binary"] not in ("+", "*"):
            raise Unsupported("UNSUPPORTED", "only + and * expressions are supported")
        return references(expr["left"]) | references(expr["right"])
    raise Unsupported("UNSUPPORTED", "unsupported expression shape")


def text(value):
    if type(value) is bool:
        return "true" if value else "false"
    if isinstance(value, str):
        return value
    if value == 0:
        return "0"
    result = repr(value)
    return result[:-2] if result.endswith(".0") else result


@dataclass(frozen=True)
class Commit:
    sequence: int
    state: tuple
    derived: tuple
    virtual_text: tuple

    def record(self):
        """Return a fresh JSON-compatible view; cannot mutate the snapshot."""
        return {"sequence": self.sequence, "state": dict(self.state),
                "derived": dict(self.derived),
                "text": {key: [value] for key, value in self.virtual_text}}


class _Program:
    def __init__(self, program):
        fields(program, {"schema_version", "profile", "scope", "declarations", "actions", "observers"})
        if type(program["schema_version"]) is not int or program["schema_version"] != 1:
            raise Unsupported("UNSUPPORTED", "unknown model schema version")
        if program["profile"] != PROFILE:
            raise Unsupported("UNSUPPORTED", "explicit S1 minimal profile required")
        identifier(program["scope"])
        self.declarations = array(program["declarations"])
        self.bindings, names = {}, set()
        for declaration in self.declarations:
            fields(declaration, {"id", "name", "kind", "expression", "source"})
            origin(declaration["source"])
            key, name = identifier(declaration["id"]), identifier(declaration["name"])
            if key in self.bindings or name in names:
                raise ModelError("E_DUPLICATE", "duplicate lexical value declaration", declaration["source"])
            if declaration["kind"] not in ("let", "get"):
                raise Unsupported("UNSUPPORTED", "only let/get declarations supported", declaration["source"])
            self.bindings[key] = declaration
            names.add(name)
        self.reads = {}
        for key, declaration in self.bindings.items():
            self.reads[key] = self.checked_reads(declaration["expression"], declaration["source"])
        initialized = set()
        for declaration in self.declarations:
            if declaration["kind"] == "let":
                for key in self.reads[declaration["id"]]:
                    if self.bindings[key]["kind"] == "get":
                        raise ModelError("E_INIT_DERIVED", "let initializer cannot read get", declaration["source"])
                    if key not in initialized:
                        raise ModelError("E_INIT_FORWARD", "initializer requires earlier let", declaration["source"])
                initialized.add(declaration["id"])
        self.validate_cycles()
        self.actions, names = {}, set()
        for action in array(program["actions"]):
            fields(action, {"id", "name", "params", "body", "source"})
            origin(action["source"])
            key, name = identifier(action["id"]), identifier(action["name"])
            if key in self.actions or name in names:
                raise ModelError("E_DUPLICATE", "duplicate lexical action", action["source"])
            if array(action["params"]):
                raise Unsupported("UNSUPPORTED", "action parameters are outside this slice", action["source"])
            for statement in array(action["body"]):
                fields(statement, {"assign", "expression", "source"})
                origin(statement["source"])
                target = identifier(statement["assign"])
                if target not in self.bindings:
                    raise ModelError("E_UNRESOLVED", target, statement["source"])
                if self.bindings[target]["kind"] != "let":
                    raise ModelError("E_DERIVED_WRITE", target, statement["source"])
                self.checked_reads(statement["expression"], statement["source"])
            self.actions[key] = action
            names.add(name)
        self.observers, names = array(program["observers"]), set()
        for observer in self.observers:
            fields(observer, {"id", "segments", "source"})
            origin(observer["source"])
            key = identifier(observer["id"])
            if key in names:
                raise ModelError("E_DUPLICATE", "duplicate text observer", observer["source"])
            names.add(key)
            for segment in array(observer["segments"]):
                self.checked_reads(segment, observer["source"])

    def checked_reads(self, expr, source):
        reads = references(expr)
        missing = reads - self.bindings.keys()
        if missing:
            raise ModelError("E_UNRESOLVED", str(sorted(missing)), source)
        return reads

    def validate_cycles(self):
        active, complete = set(), set()

        def visit(key):
            if key in active:
                raise ModelError("E_DERIVED_CYCLE", key, self.bindings[key]["source"])
            if key in complete:
                return
            active.add(key)
            for dependency in sorted(self.reads[key]):
                if self.bindings[dependency]["kind"] == "get":
                    visit(dependency)
            active.remove(key)
            complete.add(key)

        for key in sorted(self.bindings):
            if self.bindings[key]["kind"] == "get":
                visit(key)

    def evaluate(self, expr, state):
        if "literal" in expr:
            return literal(expr["literal"])
        if "read" in expr:
            key = expr["read"]
            declaration = self.bindings[key]
            if declaration["kind"] == "let":
                return state[key]
            # No cache/scheduler: substitute the formula against this exact state.
            return self.evaluate(declaration["expression"], state)
        left = self.evaluate(expr["left"], state)
        right = self.evaluate(expr["right"], state)
        if expr["binary"] == "+" and (isinstance(left, str) or isinstance(right, str)):
            return text(left) + text(right)
        if type(left) is not float or type(right) is not float:
            raise PendingPolicy("PENDING_POLICY", "S1 operand error; A6c not selected")
        result = left + right if expr["binary"] == "+" else left * right
        if not isfinite(result):
            raise PendingPolicy("PENDING_POLICY", "S1 non-finite result; A6c not selected")
        return result

    def snapshot(self, state, sequence):
        derived = tuple((key, self.evaluate(declaration["expression"], state))
                        for key, declaration in sorted(self.bindings.items())
                        if declaration["kind"] == "get")
        projected = tuple((observer["id"], "".join(text(self.evaluate(e, state))
                           for e in observer["segments"])) for observer in self.observers)
        return Commit(sequence, tuple(sorted(state.items())), derived, projected)


def run(program, steps):
    """Return initial + successful-action commits, or an explicit rejection/abstention.

    No session/resume API exists. PENDING_POLICY yields no failure-state oracle;
    local immutable maps are computation, not a selected runtime rollback policy.
    """
    try:
        model = _Program(program)
        for entry in array(steps):
            fields(entry, {"action", "args"})
            if identifier(entry["action"]) not in model.actions:
                raise ModelError("E_UNRESOLVED", entry["action"])
            if array(entry["args"]):
                raise Unsupported("UNSUPPORTED", "only zero-argument entries supported")
        state = MappingProxyType({})
        for declaration in model.declarations:
            if declaration["kind"] == "let":
                value = model.evaluate(declaration["expression"], state)
                state = MappingProxyType({**state, declaration["id"]: value})
        commits = [model.snapshot(state, 0)]
        for sequence, entry in enumerate(steps, 1):
            for statement in model.actions[entry["action"]]["body"]:
                value = model.evaluate(statement["expression"], state)
                state = MappingProxyType({**state, statement["assign"]: value})
            commits.append(model.snapshot(state, sequence))
        return tuple(commits)
    except RecursionError as error:
        raise ModelError("MODEL_LIMIT", "host recursion limit reached; no truncated result") from error
