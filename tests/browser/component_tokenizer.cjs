// Test pure emitted scanners in isolation; no dynamic compilation ships to users.
const assert = require("node:assert/strict"),
  fs = require("node:fs"),
  vm = require("node:vm");
const source = fs.readFileSync(process.argv[2], "utf8");
const functions = ["splitComponentWords", "splitTopLevelList"]
  .map((name) => {
    const start = source.indexOf("function " + name + "("),
      end = source.indexOf("\n      function ", start + 1);
    assert(start >= 0 && end > start, name);
    return source.slice(start, end);
  })
  .join("\n");
const context = vm.createContext({});
vm.runInContext(functions, context);
const words = [
  ["  when update value  ", ["when", "update", "value"]],
  ['button "a b" click run(1)', ["button", '"a b"', "click", "run(1)"]],
  ["input 'a b' into draft", ["input", "'a b'", "into", "draft"]],
  [
    'button "Send" click receive([{ text: "a b", nested: [1, 2] }], " done")',
    [
      "button",
      '"Send"',
      "click",
      'receive([{ text: "a b", nested: [1, 2] }], " done")',
    ],
  ],
  [
    'box title (enabled ? "a b" : fallback) class "雪"',
    ["box", "title", '(enabled ? "a b" : fallback)', "class", '"雪"'],
  ],
  ["x\ty\nz", ["x", "y", "z"]],
];
for (const [input, expected] of words) {
  assert.deepEqual(
    Array.from(context.splitComponentWords(input)),
    expected,
    input,
  );
}
for (const [input, expected] of [
  [
    ' a, b(c, d), [1, 2], {x: "a,b"} ',
    ["a", "b(c, d)", "[1, 2]", '{x: "a,b"}'],
  ],
  ['"escaped \\" comma, text", tail', ['"escaped \\" comma, text"', "tail"]],
  ["'雪, café', 3", ["'雪, café'", "3"]],
  ["", []],
])
  assert.deepEqual(
    Array.from(context.splitTopLevelList(input)),
    expected,
    input,
  );
console.log(
  "PASS: emitted shared token scanner quoting, nesting and comma/whitespace modes",
);
