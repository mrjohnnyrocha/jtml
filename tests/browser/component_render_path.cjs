// Pure emitted helper regression. No dynamic compilation is shipped in assets.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");
const source = fs.readFileSync(process.argv[2], "utf8");
const prefix = "withComponentRenderPath: ";
const start = source.indexOf(prefix);
assert(start >= 0, "generated creation exports the scoped path helper");
const implementation = source
  .slice(start + prefix.length, source.indexOf("\n", start))
  .trim()
  .replace(/,$/, "");
const context = vm.createContext({ componentRenderPath: [] });
vm.runInContext("this.withPath = " + implementation, context);
const path = () => Array.from(context.componentRenderPath);
assert.deepEqual(
  context.withPath("outer", () => context.withPath("inner", path)),
  ["outer", "inner"],
);
assert.deepEqual(path(), []);
const failure = new Error("deterministic child creation failure");
assert.throws(
  () =>
    context.withPath("outer", () =>
      context.withPath("inner", () => {
        throw failure;
      }),
    ),
  (e) => e === failure,
);
assert.deepEqual(path(), [], "a thrown child restores the complete prior path");
assert.deepEqual(
  context.withPath("outer", () => {
    assert.throws(
      () =>
        context.withPath("inner", () => {
          throw failure;
        }),
      (e) => e === failure,
    );
    return path();
  }),
  ["outer"],
  "inner recovery preserves the outer owner",
);
assert.deepEqual(path(), []);
assert.deepEqual(
  context.withPath("unrelated", path),
  ["unrelated"],
  "no stranded path affects subsequent owners",
);
assert.deepEqual(path(), []);
console.log(
  "PASS: scoped render path restores nested ownership on return and exception",
);
