const snippets = {
  jtml: `jtml 2

let count = 0
get doubled = count * 2

when add
  count += 1

page class "counter"
  h1 "Count: {count}"

  button "Add one" data-testid "add" click add

  text "Doubled: {doubled}" aria-label "Doubled value"`,

  html: `<main class="counter">
  <h1>Count: <span id="count">0</span></h1>

  <button data-testid="add">
    Add one
  </button>

  <p aria-label="Doubled value">
    Doubled: <span id="doubled">0</span>
  </p>
</main>

<script>
  let count = 0;
  const doubled = () => count * 2;
  document.querySelector("[data-testid='add']")
    .addEventListener("click", () => {
      count += 1;
      document.querySelector("#count").textContent = count;
      document.querySelector("#doubled").textContent = doubled();
    });
</script>`,

  react: `import { useMemo, useState } from "react";

export function Counter() {
  const [count, setCount] = useState(0);
  const doubled = useMemo(() => count * 2, [count]);

  return (
    <main className="counter">
      <h1>Count: {count}</h1>

      <button
        data-testid="add"
        onClick={() => setCount(count + 1)}
      >
        Add one
      </button>

      <p aria-label="Doubled value">
        Doubled: {doubled}
      </p>
    </main>
  );
}`
};

const code = document.querySelector("#snippet-code");
const segments = document.querySelectorAll("[data-snippet]");

function setSnippet(name) {
  code.textContent = snippets[name];
  segments.forEach((segment) => {
    const active = segment.dataset.snippet === name;
    segment.classList.toggle("is-active", active);
    segment.setAttribute("aria-selected", String(active));
  });
}

segments.forEach((segment) => {
  segment.addEventListener("click", () => setSnippet(segment.dataset.snippet));
});

setSnippet("jtml");
