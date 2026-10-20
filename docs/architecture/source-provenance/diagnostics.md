# Original-source diagnostic consumers — work in progress

Branch `codex/source-provenance-diagnostics`, worktree
`/Users/joaorocha/.codex/worktrees/source-provenance-diagnostics/jtml-v1`.
Based on propagation `d888bcd`, which still has final validation in progress.

Static compiler diagnostics now consume explicit original ranges carried by the
owning Runtime Plan body node. No matching by component name and no numeric
absolute-versus-relative heuristic. Unknown compatibility ranges are explicitly
labelled lowered; transformed nodes are labelled attributed generated code.
The original-location records are removed at static asset emission for both
production and compatibility output. No strict N04 PASS claim until the unchanged
oracle and independent adversarial tests execute.

Next: build final consumer, run strict N04 unchanged; add imports/re-exports,
coordinate variation and all-path rejection oracles. Audit same-name definitions
and generated nodes for false attribution. Native corruption remains unresolved.

Unchanged strict N04 PASS on the consumer binary; saved propagation-only binary
still exits 2 with the same original fixtures. Seven further adversarial cases
pass: CRLF/Unicode/blank-line shift, re-exported selected imports, same-named
components in two files, original scope leak, duplicate writes at distinct lines,
imported tab rejection at its original file, and multiline Classic lexer error.
These tests derive coordinates from original files and verify no runnable assets.
The final local gate passed 388/388 CTests, nine clean-exit benchmarks and all
existing browser/native gates. A test-only extension checking original-location
exclusion in both static assets requires a focused rebuild before final commit.
This proves these diagnostic paths; it does not claim every legacy string-based
compiler/runtime diagnostic has been migrated. Fine-grained Friendly expression
origins remain generated attribution when lowering does not preserve exact bytes.
