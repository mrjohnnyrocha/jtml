# 6. Reactive Side Effects

Friendly `effect var` runs the indented block every time `var` changes. It's the imperative escape hatch for side effects that can't be expressed as a `get` value: logging, persistence, triggering external calls.

The example appends to `history` whenever `count` changes. Press **Bump** a few times and watch the log grow.

Internally, `effect` lowers to a subscription. Subscriptions are deduplicated, so you cannot accidentally register the same callback twice.
