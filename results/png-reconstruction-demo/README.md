# PNG reconstruction milestone validation

The complete prototype was built and executed in the development environment.
No user-machine testing was required. Larger-volume scaling remains suspended.

- `end-to-end-validation.json`: **141 passing checks**, **70 actual CLI
  invocations**, seven independent exhaustive image comparisons (6,561 worlds
  each), exact supports, every oracle-case witness re-rendered against every
  input pixel, deterministic output, PNG variants, malformed-input and UNSAT checks.
- `end-to-end.log` / `end-to-end.xml`: passing CTest result for the file pipeline.
- `regressions.log` / `regressions.xml`: **14/14 focused A0/A1/A2 tests passed**.
  These exercise geometry, rendering, propagation, feasibility, support queries,
  projection, structured inference and compatibility. CTest truncates lengthy
  per-test stdout in the XML; pass/fail status is complete.
- `application-build.log`: final application rebuild after PNG validation changes.
- `demo-execution.json`: explicit executions of both committed PNG/camera pairs;
  output hashes match the committed reconstruction files.
- `build-identity.json`: actual compiler/toolchain, binary hash, source identities
  and runtime dependencies. This describes a product correctness build, not a
  frozen scientific timing environment.
- `mcr_reconstruct-linux-x86_64.tar.gz`: the tested executable plus LodePNG and
  PicoJSON license notices. Reproduce with `tools/package_png_demo.py`.
- `checkpoint-integrity.json`: hashes of the new checkpoint files and proof that
  prior core, reference, experiment, input and evidence files remain unchanged.

Reproduce with the `png-demo` configure/build/test presets documented in
[the guide](../../docs/png-reconstruction-demo.md). The two ordinary example
executions are in addition to the 70 end-to-end test invocations. No timing study,
benchmark, historical acceptance-corpus generation or larger-volume case ran.
Test durations in CTest logs are execution records, not performance evidence.

During development a missing-image error initially returned the generic I/O exit
code when an old output existed; the path check was corrected and the final suite
verifies input error code 2 while preserving old output. No solver change was needed.

The visible scene has one feasible world; the occluded scene has 81. These counts
come from the independent test oracle and are not exposed as a production world
count. The application returns exact marginal supports and one validated witness.
