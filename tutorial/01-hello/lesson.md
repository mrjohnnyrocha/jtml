# 1. Hello, JTML

This is canonical Friendly JTML. The header `jtml 2` enables the friendly dialect. `let` declares state, `page` is the root element, and `{name}` interpolates a value into a string.

The program below does three things:

1. Declares a reactive variable `name` with `let`.
2. Greets that variable inside an `h1` using `{name}` interpolation.
3. Wraps everything in a `page` element with inline `style`.

Try editing the string `"world"` and pressing **Run**. The page rebuilds.
