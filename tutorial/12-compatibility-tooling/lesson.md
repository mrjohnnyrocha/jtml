# 12. Friendly JTML 2, Classic, And Tooling

For new code, choose **Friendly JTML 2**:

```jtml
jtml 2

let count = 0

when add
  count += 1

page
  button "Add" click add
```

Classic JTML remains supported for old files, compiler artifacts, embedding, and migration:

```jtml
define count = 0\\
function add()\\
  count += 1\\
\\
@button onClick=add()\\
  show "Add"\\
#
```

Use Classic when maintaining old code or inspecting the Artifacts panel. Use Friendly everywhere else.

Studio maps the ecosystem:

- **Run** compiles, lints, and previews.
- **Lint** checks source without changing it.
- **Fix** applies safe mechanical repairs.
- **Format** writes canonical Friendly style.
- **Artifacts** shows lowered Classic and generated HTML.
- **Export** produces a standalone HTML artifact.
