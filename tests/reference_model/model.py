"""Independent, deliberately small scalar model; no source parser or file IO."""
from dataclasses import dataclass
from itertools import permutations
from math import copysign, fmod, isfinite
import operator
import re
from types import MappingProxyType

PROFILE = "s1-minimal-success-v1"
EXPERIMENT = "strict-root-scalar-episodes-v1"
P3 = "experimental-P3"
NUMERIC = {"+": operator.add, "-": operator.sub, "*": operator.mul, "/": operator.truediv, "%": fmod}
ORDER = {"<": operator.lt, "<=": operator.le, ">": operator.gt, ">=": operator.ge}


class ScalarFault(Exception):
    pass


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


def expression_kind(expr, read, strict, infer=True):
    """Visit both branches statically; infer kinds or just collect resolved reads."""
    if not isinstance(expr, dict):
        raise ModelError("E_SCHEMA", "expression must be an object")
    keys = set(expr)

    def visit(child):
        return expression_kind(child, read, strict, infer)

    if keys == {"literal"}:
        value = literal(expr["literal"])
        if strict and type(value) is float:
            lexical = expr["literal"]["value"]
            if not re.fullmatch(r"-?(?:0|[1-9][0-9]*)", lexical) or abs(value) > 2**53 - 1:
                raise ModelError("E_UNSUPPORTED_EXPR", "numeric literal outside the safe integer slice")
        if strict and type(value) is str:
            if any(not " " <= ch <= "~" or ch in '"{}' or ch == chr(92) for ch in value):
                raise ModelError("E_UNSUPPORTED_EXPR", "string literal outside the plain ASCII slice")
        return type(value)
    if keys == {"read"}:
        return read(identifier(expr["read"]))
    if keys == {"binary", "left", "right"}:
        op = identifier(expr["binary"])
        if not strict and op not in ("+", "*"):
            raise Unsupported("UNSUPPORTED", "only + and * expressions are supported")
        left, right = visit(expr["left"]), visit(expr["right"])
        if not infer:
            return None
        if left is right:
            if op in ("==", "!="):
                return bool
            if op in ("&&", "||") and left is bool:
                return bool
            if op in ORDER and left is float:
                return bool
            if (op in NUMERIC and left is float) or (op == "+" and left is str):
                return left
    if strict and keys == {"unary", "operand"}:
        expected = {"-": float, "!": bool}.get(identifier(expr["unary"]))
        actual = visit(expr["operand"])
        if expected is not None and (not infer or expected is actual):
            return expected
    if strict and keys == {"select", "then", "else"}:
        condition, left, right = visit(expr["select"]), visit(expr["then"]), visit(expr["else"])
        if not infer or (condition is bool and left is right):
            return left
    if strict:
        raise ModelError("E_UNSUPPORTED_EXPR", "unsupported scalar expression or operand kinds")
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
        self.strict = program.get("profile") == EXPERIMENT if isinstance(program, dict) else False
        fields(program, {"schema_version", "profile", "scope", "declarations", "actions", "observers"}
               | ({"effects"} if self.strict else set()))
        if type(program["schema_version"]) is not int or program["schema_version"] != 1:
            raise Unsupported("UNSUPPORTED", "unknown model schema version")
        if program["profile"] not in (PROFILE, EXPERIMENT):
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
        self.kinds = {}
        if self.strict:
            for key in self.bindings:
                self.binding_kind(key)
        self.actions, names = {}, set()
        for action in array(program["actions"]):
            fields(action, {"id", "name", "params", "body", "source"})
            origin(action["source"])
            key, name = identifier(action["id"]), identifier(action["name"])
            if key in self.actions or name in names:
                raise ModelError("E_DUPLICATE", "duplicate lexical action", action["source"])
            params = {identifier(param) for param in array(action["params"])}
            if not self.strict and params:
                raise Unsupported("UNSUPPORTED", "action parameters are outside this slice", action["source"])
            if len(params) != len(action["params"]) or params & self.bindings.keys():
                raise ModelError("E_SCHEMA", "parameter identities must occupy a separate frame", action["source"])
            self.body_writes(action["body"], params)
            self.actions[key] = action
            names.add(name)
        self.effects = self.effect_order(program["effects"]) if self.strict else ()
        self.observers, names = array(program["observers"]), set()
        for observer in self.observers:
            fields(observer, {"id", "segments", "source"})
            origin(observer["source"])
            key = identifier(observer["id"])
            if key in names:
                raise ModelError("E_DUPLICATE", "duplicate text observer", observer["source"])
            names.add(key)
            for segment in array(observer["segments"]):
                if self.strict:
                    if set(segment) not in ({"literal"}, {"read"}):
                        raise ModelError("E_UNSUPPORTED_EXPR", "only reviewed text segments", observer["source"])
                    self.check_kind(segment, observer["source"], self.binding_kind)
                else:
                    self.checked_reads(segment, observer["source"])

    def checked_reads(self, expr, source):
        reads = set()
        try:
            expression_kind(expr, reads.add, self.strict, infer=False)
        except ModelError as error:
            error.source = source
            raise
        missing = reads - self.bindings.keys()
        if missing:
            raise ModelError("E_UNRESOLVED", str(sorted(missing)), source)
        return reads

    def check_kind(self, expr, source, read):
        try:
            return expression_kind(expr, read, self.strict)
        except ModelError as error:
            if error.source is None:
                error.source = source
            raise

    def binding_kind(self, key):
        if key not in self.bindings:
            raise ModelError("E_UNRESOLVED", key)
        if key not in self.kinds:
            decl = self.bindings[key]
            self.kinds[key] = self.check_kind(decl["expression"], decl["source"], self.binding_kind)
        return self.kinds[key]

    def body_writes(self, body, params):
        writes = set()

        def kind(key):
            return float if key in params else self.binding_kind(key)

        for stmt in array(body):
            if not isinstance(stmt, dict) or "source" not in stmt:
                raise ModelError("E_SCHEMA", "statement requires a source location")
            source = stmt["source"]
            origin(source)
            if self.strict and "show" in stmt:
                fields(stmt, {"show", "source"})
                self.check_kind(stmt["show"], source, kind)
            elif self.strict and "if" in stmt:
                fields(stmt, {"if", "then", "else", "source"})
                if self.check_kind(stmt["if"], source, kind) is not bool:
                    raise ModelError("E_UNSUPPORTED_EXPR", "condition requires Boolean", source)
                writes |= self.body_writes(stmt["then"], params) | self.body_writes(stmt["else"], params)
            else:
                fields(stmt, {"assign", "expression", "source"})
                target = identifier(stmt["assign"])
                if target not in params:
                    if target not in self.bindings:
                        raise ModelError("E_UNRESOLVED", target, source)
                    if self.bindings[target]["kind"] != "let":
                        raise ModelError("E_DERIVED_WRITE", target, source)
                    writes.add(target)
                if self.strict:
                    if self.check_kind(stmt["expression"], source, kind) is not kind(target):
                        raise ModelError("E_UNSUPPORTED_EXPR", "assignment changes scalar kind", source)
                else:
                    self.checked_reads(stmt["expression"], source)
        return writes

    def effect_order(self, entries):
        effects, writes = {}, {}
        if len(array(entries)) > 6:
            raise ModelError("MODEL_LIMIT", "oracle enumerates at most six effects")
        for effect in entries:
            fields(effect, {"id", "watch", "body", "source"})
            origin(effect["source"])
            key, watch = identifier(effect["id"]), identifier(effect["watch"])
            if key in effects:
                raise ModelError("E_DUPLICATE", key, effect["source"])
            if watch not in self.bindings or self.bindings[watch]["kind"] != "let":
                raise ModelError("E_UNRESOLVED", watch, effect["source"])
            effects[key] = effect
            writes[key] = self.body_writes(effect["body"], set())
        # Search complete orders lexicographically by source position. Never ask
        # the production scheduler which callbacks are ready or how ties break.
        source_order = sorted(effects, key=lambda key: (effects[key]["source"]["line"], effects[key]["source"]["column"]))
        for candidate in permutations(source_order):
            if all(effects[watcher]["watch"] not in writes[writer] or i < j
                   for i, writer in enumerate(candidate) for j, watcher in enumerate(candidate)):
                return tuple(effects[key] for key in candidate)
        raise ModelError("E_EFFECT_CYCLE", "potential effect feedback", effects[source_order[0]]["source"])

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

    def evaluate(self, expr, state, frame=None):
        def value(child):
            return self.evaluate(child, state, frame)

        if "literal" in expr:
            return literal(expr["literal"])
        if "read" in expr:
            key = expr["read"]
            if frame is not None and key in frame:
                return frame[key]
            declaration = self.bindings[key]
            if declaration["kind"] == "let":
                return state[key]
            # Substitute the formula under its declaration's scope and current W.
            return self.evaluate(declaration["expression"], state)
        if "unary" in expr:
            operand = value(expr["operand"])
            return -operand if expr["unary"] == "-" else not operand
        if "select" in expr:
            return value(expr["then"] if value(expr["select"]) else expr["else"])
        op = expr["binary"]
        left = value(expr["left"])
        if op == "&&" and not left:
            return False
        if op == "||" and left:
            return True
        right = value(expr["right"])
        if op in ("&&", "||"):
            return right
        if op == "==":
            return left == right
        if op == "!=":
            return left != right
        if op in ORDER:
            return ORDER[op](left, right)
        if op == "+" and (isinstance(left, str) or isinstance(right, str)):
            return text(left) + text(right)
        if type(left) is not float or type(right) is not float:
            raise PendingPolicy("PENDING_POLICY", "S1 operand error; A6c not selected")
        if op in ("/", "%") and right == 0:
            raise ScalarFault("E_SCALAR_DOMAIN")
        result = NUMERIC[op](left, right)
        if not isfinite(result):
            if self.strict:
                raise ScalarFault("E_SCALAR_DOMAIN")
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
        if model.strict:
            raise Unsupported("UNSUPPORTED", "use run_trace with explicit experimental policy")
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


def typed(value):
    if type(value) is float:
        return {"kind": "number", "value": "-0" if value == 0 and copysign(1, value) < 0 else value}
    return {"kind": "boolean" if type(value) is bool else "string", "value": value}


def typed_record(record):
    return {key: typed(value) for key, value in record}


def trace_snapshot(model, state, sequence):
    commit = model.snapshot(state, sequence)
    return {"sequence": sequence, "state": typed_record(commit.state),
            "derived": typed_record(commit.derived), "dom": commit.record()["text"].get(".out", [])}


def run_trace(program, steps, failure_policy=None):
    """Trace the fixture experiment with explicit policy selection, or abstain."""
    if failure_policy is None:
        return {"validation": "PENDING_POLICY"}
    if failure_policy != P3 or program["profile"] != EXPERIMENT:
        raise Unsupported("UNSUPPORTED", "explicit strict root scalar experiment and P3 required")
    try:
        model = _Program(program)
        for entry in array(steps):
            fields(entry, {"action", "args"})
            action = model.actions.get(identifier(entry["action"]))
            args = array(entry["args"])
            if action is None or len(args) != len(action["params"]):
                raise ModelError("E_SCHEMA", "unknown fixture entry or wrong arity")
            for node in args:
                if set(node) not in ({"literal"}, {"read"}) or model.check_kind(
                        node, action["source"], model.binding_kind) is not float:
                    raise ModelError("E_UNSUPPORTED_EXPR", "entry requires a numeric literal/read", action["source"])
        return _run_trace(model, steps)
    except Unsupported:
        raise
    except ModelError as error:
        if error.code in ("MODEL_LIMIT", "E_SCHEMA"):
            raise
        return {"validation": "reject", "code": error.code, "source": error.source}
    except RecursionError as error:
        raise ModelError("MODEL_LIMIT", "host recursion limit reached; no truncated trace") from error


def _run_trace(model, steps):
    state = {}
    try:
        for declaration in model.declarations:
            if declaration["kind"] == "let":
                state[declaration["id"]] = model.evaluate(declaration["expression"], state)
        committed = trace_snapshot(model, state, 0)
    except ScalarFault:
        return {"validation": "unsupported", "reason": "Fault during initialization is outside this oracle"}

    result = {"validation": "accept", "initial": committed, "steps": []}
    failed = False
    for step in steps:
        if failed:
            result["steps"].append({"outcome": "dispatch-rejected", "events": [], "lastCommit": committed})
            continue
        action = model.actions[step["action"]]
        frame = {key: model.evaluate(expr, state) for key, expr in zip(action["params"], step["args"])}
        working, written, events = dict(state), set(), []
        phase, active_effect = action["id"], None

        def execute(body, locals_):
            def value(expr):
                return model.evaluate(expr, working, locals_)

            for stmt in body:
                if "if" in stmt:
                    execute(stmt["then"] if value(stmt["if"]) else stmt["else"], locals_)
                elif "show" in stmt:
                    shown = value(stmt["show"])
                    events.append({"kind": "show", "phase": phase, "source": stmt["source"],
                                   "scalar": typed(shown), "text": text(shown)})
                else:
                    target, new = stmt["assign"], value(stmt["expression"])
                    if target in locals_:
                        locals_[target] = new
                    else:
                        working[target] = new
                        written.add(target)

        try:
            execute(action["body"], frame)
            for effect in model.effects:
                watch = effect["watch"]
                if watch not in written:
                    continue
                active_effect = phase = effect["id"]
                payload = {"effect": phase, "watch": watch, "scalar": typed(working[watch])}
                events.extend({"kind": kind, **payload} for kind in ("notify", "effect-enter"))
                execute(effect["body"], {})
                events.append({"kind": "effect-exit", "effect": phase, "outcome": "ok"})
                active_effect = None
            phase = "publication"
            committed = trace_snapshot(model, working, committed["sequence"] + 1)
            state = working
            result["steps"].append({"outcome": "commit", "events": events, "commit": committed})
        except ScalarFault as error:
            if active_effect is not None:
                events.append({"kind": "effect-exit", "effect": active_effect, "outcome": "error"})
            failed = True
            result["steps"].append({"outcome": "episode-error", "policy": P3, "code": str(error),
                                    "action": action["id"], "phase": phase, "events": events,
                                    "failedPartial": typed_record(working.items()), "lastCommit": committed})
    return result
