# Stable-host and anchor-stage preparation

**No experimental execution; no stable collection host selected.**
See [the deployment guide](../../docs/a2-scaling-larger-stable-host.md).

- `anchors/selection.json`, selected-ID archive and completion marker: exactly
  four original physical-anchor IDs from the 72,148-ID master manifest.
- `anchors/dry-run.json`: five planned job/epoch entries and 640 planned timed
  calls; zero executed calls.
- `anchor-deployment-validation.json`: 43 passing nonexperimental checks,
  including explicit rejection of non-anchor N=8 designs and unsafe resume.
- `workspace-host-check.json`: bootstrap/dependency/capability check on the
  current ephemeral workspace, not a stable-host freeze or collection authority.
- `validation-implementation-commit.txt`: exact local commit bytes for the
  checkout used by that check; its complete tree matches the published
  implementation commit, as recorded in the integrity manifest.
- `checkpoint-integrity.json`: file sizes, SHA-256 hashes and Git blob identities
  for this checkpoint, excluding that manifest itself.

The intended sequence is stable-host freeze and publication, anchors, publication
and verification of anchor evidence, then N=4/6, with later batches on the same
host. Earlier blocked launches, original preflight records and all scientific
artifacts remain intact. No current workspace host record authorizes collection.
