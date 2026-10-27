# Reproduction evidence

`calibration.tar.gz` contains every local pilot attempt and the minimal reset
probe, including source/output snapshots, build logs, result JSON and browser
traces/screenshots where available. `SHA256SUMS` pins the archive and summaries.
`final-results.json` is a convenient copy of the final calibration matrix.
These are harness/reference results, not agent trials or adoption activity.

Extract in a temporary directory. Open trace ZIPs with
`npx playwright show-trace /absolute/path/to/trace.zip`. To rerun the final
benchmark, use the checked-in package lock, the compiler hash in REPORT.md,
and the README commands; output paths and timings will differ.

The archive excludes dependency symlinks and includes final evaluator source.
No failed pilot is removed. Unit-test telemetry fixtures are never included as
agent records. `verification.json` reports repository-wide verification separately.
