# 4. Live Input Binding

The Friendly `into name` syntax binds a form input to a reactive variable in one direction (UI → state) without a separate handler. Every keystroke updates `name`, and every `{name}` interpolation rebuilds.

This gives you controlled-input behaviour for free: the page state is always the source of truth, and the UI updates reactively. Type into the input and watch the greeting rebuild in real time.
