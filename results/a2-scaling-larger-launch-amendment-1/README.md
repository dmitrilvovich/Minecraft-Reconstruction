# Pre-collection infrastructure revision 1

**Collection not cleared; zero experimental execution.** See the
[amendment and remaining blockers](../../docs/a2-scaling-larger-launch-amendment-1.md).

- `collection-environment-status.json`: observed host history; explicitly
  unselected for collection. A reserved stable environment is still required.
- `n04-n06/selection.json`: exact complete-stage subset of the original
  72,148-ID master manifest; 3,888 N=4 and 12,384 N=6 IDs, total **16,272**.
- `n04-n06/selected-design-ids.jsonl.gz` and completion marker: original IDs,
  archive locations, data-record ordinals and raw-record hashes; no regenerated
  scientific inputs. Original input bytes remain in the v1 archives.
- `n04-n06/dry-run.json`: 195 planned job/epoch entries, **737,280 planned
  calls**, ordered-ID hashes, **zero calls executed**.
- `n04-n06/validation.json`: 37 passing nonexperimental boundary/integrity
  checks. No solver, oracle, timing epoch or preflight rerun.
- `checkpoint-integrity.json`: SHA-256, size and Git blob identity of all new
  published files except this self-referential manifest.

The unmodified v1 schedule requires all four N=8 physical anchors first. They
are not in the N=4/6 batch. The scoped launcher stops on missing prerequisites
rather than running them outside the batch. A separate anchor-order decision
and a frozen stable collection environment are required before launch.
