# Versioned larger-volume launch repair

See [launch amendment 1](../../docs/a2-scaling-larger-launch-amendment-1.md).
This directory is separate from the preserved `experiments/a2_larger` freeze.
**No collection is authorized or cleared by this checkpoint.**

`selection.py` reads frozen inputs and creates an index plus a nonexecuting
five-epoch dispatch inventory. It accepts whole volumes (`--volumes 4 6`) or
explicit complete stages (`--stages anchors:8`, `volume:4`, etc.). The already
published N=4/6 selection is in
`results/a2-scaling-larger-launch-amendment-1/n04-n06/`; reuse it rather than
rebuilding it at launch. `validate.py` performs only infrastructure checks and
never creates a real worker or independent oracle.

`collect.py` is the scoped execution path for a **separately authorized future
checkpoint**, using mandatory `--selection`, `--collection-environment`,
`--collection-authorized-checkpoint`, `--cpu` and shared study `--output` arguments.
It imports the original frozen worker, references and timing-order functions.
All batches must share the same output root and budget ledger. It rejects an
unselected/changed timing environment, unpublished or changed launch artifacts,
missing prerequisite stages, changed input identities and out-of-scope dispatches.

The original protocol explicitly puts the N=8 anchors first. This launcher
preserves that requirement: an N=4/6-only request cannot run them implicitly and
therefore cannot be the first executed stage without a separate prospective
ordering decision. No new solver binary was built.
