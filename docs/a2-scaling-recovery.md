# A2 eight-cell scaling recovery

**Adoption status:** the surviving 491,199-case run is now the canonical
reconstructed exploratory dataset. It is not the recovered original, is not
proven equivalent, and is not a verified correction or superset. The
[adoption record](a2-scaling-adoption.md) governs dataset identity, reconciliation
caveats, and the **not yet scientifically reviewed** status of existing analysis.
The account below preserves the earlier recovery and reconstruction history.

The September 29 branch resumed from accepted commit
`e73b710a42bca1cd0c7a69b9b9c37db2595a1cb1`. Recovery checked the accessible
workspace and temporary directories, worktrees, index, reflogs, unreachable Git
objects, saved bundles, remote `main`, and stored-file search. The checkout was
clean. None of the interrupted scaling sources, dataset, analysis, charts, or
logs was accessible. The accepted production code, frozen reference, correctness
reports, and hash-matching A2 Python image fixtures survived.

The user was informed of this before reconstruction. The historical commentary
reported 410,953 cases, 16,660,932 verified witnesses, and no unresolved calls.
Those are **unrecovered historical claims**, not counts established by this
replacement collection. Historical timings and exact population cannot be
recreated from commentary. In particular the old 30 mask values, six suite names,
sampling seeds, perturbation recipe, timing/query sampling, and aggregation
policy were unavailable. The replacement collector does not filter cases to
match the historical count; the unavailable original protocol prevents a claim
of equivalence or original preregistration.

The replacement stays within the declared scope: eight cells, 30 placements
over k=0..8, paired palettes, six suites, 16x16 controls at k=0,4,8,
restriction/observation perturbations, structured controls, four solver modes,
one million search-node entries or 60 seconds per call. The explicit replacement
choices are in `results/a2-scaling-8cell/protocol.json`, written before collection.
They are not represented as the original preregistration. The secondary
occupancy-balanced weighting was already requested by the interrupted analysis;
it uses the same measured families and introduces no additional solver cases.

Every job writes to memory-backed temporary storage. After the process closes
its outputs, the driver checks case IDs, complete truth and balanced probability
mass, expected API calls, statuses, deterministic work counters across repeats,
and recorded witness totals. It then writes deterministic gzip files, decompresses
them to compare SHA-256, and atomically publishes each archive and finally its
completion manifest. Resuming verifies both compressed and uncompressed hashes.
Only absent jobs are run. The final collection marker requires every declared job.

The completed replacement contains 491,199 cases. Final transport uses lossless
columnar encoding of the 148 CSV tables, with every reconstructed CSV compared
byte-for-byte by SHA-256, then a hash-checked split tar archive. The complete
unpacker was run and restored the original compressed CSV hashes as well. This
storage conversion performs no inference and drops no measurements.

A later reread found six truncated files in the temporary unpacked copy after
the unpacker had passed its in-process checks. Every original compressed shard
and transport part still matched its recorded hash. Only those temporary copies
were restored from the verified originals; no solver work was repeated. The
transport parts were staged immediately and their Git-index bytes checked
against the manifest. `validation.json` records the affected filenames and sizes.

No accepted source, frozen Python, or prior correctness evidence is modified.
No full correctness/sanitizer gate or larger-volume study is rerun. The initial
CMake configure attempt could not run because CMake is absent in this branch's
runtime; the saved shell build uses the installed compiler with explicit Release
flags and compiles the unchanged source tree.
