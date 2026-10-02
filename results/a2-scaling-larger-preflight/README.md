# Infrastructure-only preflight evidence

The [fixed preflight plan](../../experiments/a2_larger/preflight-plan.json) covers
40 design cases. It is separate from the 72,148-slot collection; **zero scientific
timing epochs and zero main-dataset solver calls were executed**.

- [Successful summary](attempt-02-summary.json), [host identity](host.json), and
  [successful evidence archive](attempt-02.tar.gz).
- [Preserved failed-attempt summary](attempt-01-summary.json) and
  [failed evidence archive](attempt-01.tar.gz).
- [Archive manifest](archive-manifest.json): exact sizes/hashes and verified internal
  file counts. Extract archives under this directory to recover the original
  `attempt-01/` and `attempt-02/` paths.

The successful attempt contains 368 ordinary API calls, two root diagnostics,
four deliberately exhausted node/time calls and three non-solver fault requests.
The previous attempt made one request and stopped on malformed transport JSON.
The experiment-layer helper was corrected; accepted solver/reference code and
the scientific protocol were unchanged. Total checkpoint requests: **378**, below
the fixed ceiling of 400. The failed attempt was not used as scientific evidence.

Each archive contains `FILES.json`, recording every other member's exact size and
SHA-256. Packaging fixes gzip mtime, tar mtime, owner/group and file modes. Closed
JSONL artifacts additionally have record counts and compressed/uncompressed hashes.
The intentionally corrupt gzip and incomplete JSONL in `storage-tests/` are fault
fixtures: their bytes are preserved and hashed, but they are deliberately not valid
completed job artifacts. Temporary process-control pages and the derived comparison
database are retained for audit, not treated as scientific measurements.

The successful source identities match the frozen collector/reference package.
The failed archive includes its exact earlier source snapshot. Every returned
witness from the valid ordinary calls was independently checked. No performance
trend, optimization benefit or larger-volume conclusion is inferred here.
