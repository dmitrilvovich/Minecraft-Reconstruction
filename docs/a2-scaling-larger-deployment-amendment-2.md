# Deployment amendment 2: build provenance and timing-host qualification

**Prospective infrastructure revision, before collection. Zero experimental
solver calls, reference jobs or timing epochs have occurred. No collection is
authorized by this amendment.** Base: `e97883ab3a982292af98e5e3ed2a3b1fe5258f54`.
The scientific v1 protocol, all 72,148 input IDs, anchor and N=4/6 selections,
accepted solver, frozen Python reference and original evidence remain unchanged.

Use [deployment v2](../experiments/a2_larger_deploy_v2/README.md) for future host
inspection and authorized collection. The [machine-readable amendment](../experiments/a2_larger_deploy_v2/spec.json)
explicitly supersedes the execution-compiler requirement in the preserved v1
deployment. The original protocol, host-policy amendment 1 and deployment v1
are retained byte-for-byte; their historical requirements are not silently edited.

## Why the compiler stop is unnecessary for execution

The original protocol's `freeze.build` and build section require a frozen
compiler, build command, executable and collection environment. Amendment 1
separates preflight hardware from the prospectively chosen collection host.
Deployment v1 then required the compiler **installed on that collection host**
to be the original build compiler. This last equality is unnecessarily strict
for executing the already-built, hash-verified ELF. It is now explicitly amended,
rather than claimed never to have been required.

Source inspection confirms that `experiments/a2_larger/build.sh` is the build
step. The collection `Worker` directly starts the frozen executable; the
deployment and collection paths do not invoke the build script, generate code
or compile at runtime. `g++ --version` was an identity probe. The ELF needs its
loader and shared runtime libraries, including libstdc++; it does not need the
compiler driver that produced it. An installed compiler package can update
runtime libraries, which is why those libraries must still be checked and frozen
separately. Accepting a different driver does not waive any runtime requirement.

| Identity | Policy in deployment v2 |
| --- | --- |
| Original build compiler | Immutable version `g++ (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0`; SHA-256 `52f1ddb33fe78b9441e0f42e9cd22c571f1101938e046c8a26582494e041cc73`. No need to install it on the execution host. |
| Original build record | Exact bytes retained; SHA-256 `aebef728ba64fb9ad8f6d26d4b7fc3677603e048f4f84b298474cedbfa8f4b22`. Original flags, sources, build host and compiler remain provenance. |
| Frozen executable | Exact SHA-256 `882d5025cc9f1334e78a5c39cfdd46b80aa255fa97704d4e71abbd53234daf1f` required. Restore published bytes only; never rebuild or replace a mismatching binary. |
| Execution-host compiler | Record `g++` resolved path, version and SHA-256 if present, or explicit absence. No comparison to the build compiler. A present but unidentifiable driver blocks the check. |
| Execution runtime | Required ELF symbol versions must resolve. Freeze actual loader/shared-library hashes, Python/NumPy identities, kernel/OS, machine/topology, affinity, limits and timing environment. |
| Later batches | Exact environment comparison includes the installed compiler's presence/path/version/hash. Installation, removal, upgrade, library change or other captured identity drift stops before further workers. Never rewrite a frozen record to accept drift. |

The user's GCC revision `13.3.0-6ubuntu2~24.04.1`, SHA-256
`1353e9bdd29a7295c7226bf6c63abccce056d8cac31f112e5cdbecc3f28c2769`, is therefore
not intrinsically a blocker. That identity is a user-reported execution-host
observation, **not a replacement build hash**. Its acceptance by the revised
policy does not qualify the user's timing environment. No package downgrade or
rebuild is needed to resolve this particular check.

## WSL2 on the i7-13700H: not qualified for this timing study

Intel specifies six performance cores, eight efficiency cores and 20 threads
for the [i7-13700H](https://www.intel.com/content/www/us/en/products/sku/232128/intel-core-i713700h-processor-24m-cache-up-to-5-00-ghz/specifications.html).
Twenty Linux CPUs exposed by WSL do not establish a fixed mapping to those
physical cores. Microsoft describes [WSL2 as a managed utility VM](https://learn.microsoft.com/en-us/windows/wsl/compare-versions).
The documented [`.wslconfig` `processors` setting](https://learn.microsoft.com/en-us/windows/wsl/wsl-config)
sets the processor count, not an individual guest-to-physical-core binding.

Microsoft's [Hyper-V scheduler documentation](https://learn.microsoft.com/en-us/windows-server/virtualization/hyper-v/manage/manage-hyper-v-scheduler-types)
distinguishes guest virtual processors from host logical processors: guest work
can be scheduled on different host processors. On Windows client systems the
root scheduler delegates scheduling to Windows; Microsoft advises against
changing that scheduler type. A Linux affinity mask constrains the guest
scheduler. It is not evidence that Windows consistently runs that virtual CPU
on one physical P-core or E-core.

**Assessment:** the supplied WSL installation has not demonstrated sufficiently
controlled physical placement for this protocol's cross-volume wall-clock
comparisons. No supported, verified WSL placement mechanism has been established
for this laptop. `taskset`, reducing WSL processor count, or a Windows process
affinity setting alone are not accepted as such proof. The conclusion is about
qualification for this study, not a claim that WSL can never measure performance.
The reported RAM and disk satisfy the nominal thresholds, subject to the live
effective-resource checks; they do not resolve CPU placement.

Deployment v2 detects WSL from kernel metadata, WSL environment signals and,
when available, `systemd-detect-virt`. It can complete the metadata, hash,
runtime and synthetic capability inspection there. If those pass, it reports
`infrastructure_status: PASS` **and** overall `status: BLOCKED` for timing
qualification. It cannot prepare a WSL host record, and the collection guard
also rejects WSL, even if given an otherwise matching record. There is no WSL
override flag. No actual collection host is frozen by this checkpoint.

The practical recommendation is native Ubuntu 24.04 on reserved physical
hardware: identify the selected physical core and its type (prefer an identified
P-core on this hybrid laptop), pin to its Linux logical CPU, control sibling
load/background work, and record consistent AC power, governor/turbo and thermal
conditions before freezing. Do not assume CPU 0 is a P-core without inspection.
A persistent dedicated VM is an alternative only with independently reviewed
host-side vCPU-to-core binding, backing-host identity and non-migration evidence;
guest affinity alone remains insufficient. A host/provider/operator must enforce
those reservations and controls across batches; guest metadata cannot prove
hidden host state. Any proposed WSL solution needs a separate prospective review
before enabling collection. No retrospective heterogeneous-host normalization.

Official sources above were inspected on 2026-10-21. The timing qualification is
an inference from their scheduling model and the absence of placement evidence
for this host; no timing experiment was conducted to reach it.

## Implementation, validation and unchanged obligations

The new `experiments/a2_larger_deploy_v2/` directory preserves the original
deployment implementation and reuses its binary restore, ELF dependency/symbol,
resource, cgroup, affinity/memory/watchdog capability checks. The complete
`collect.py` is byte-identical to deployment v1; its script-local import selects
the v2 environment guard. No scientific collection body or ordering rule changed.
Host records use schema `a2-larger-stable-collection-host-v2`; v1 records are not
silently reinterpreted. V2 also requires physical-placement evidence when
preparing a prospective record. The evidence is an operator attestation/reference
requiring review before publication, not an automatic proof of physical binding.

The [bounded validation](../results/a2-scaling-larger-deployment-v2/validation.json)
contains **48 passing checks**. It exercises actual metadata capture with a
different compiler identity and with no compiler, immutable build/binary rejection,
post-freeze compiler/runtime/source/machine drift rejection, WSL detection and
freeze/collection rejection, retained RAM/disk stops, and unchanged orchestration.
It verifies original artifact hashes, including all 40 input archives. The
compiler-difference case injects the reported driver metadata into local capture;
it is not an execution on the user's laptop. Synthetic host records and mocked
publication checks exist only in the test. No real host approval was produced.

Zero experimental solver calls, reference jobs, physical anchors or timing epochs
were run. The old 40-case preflight was not rerun. No new scientific inputs,
performance estimates, correctness gates, benchmarks, analysis or charts were
produced. The original preflight failure/success and blocked-launch records remain.

Node/time/watchdog/worker limits remain 1,000,000 / 60 s / 65 s / 4 GiB;
exhaustion stays `UNRESOLVED`. The 20 GiB reserve, reference obligations, five
epochs, configurations, branching, seeds, weighting and pause rules are unchanged.
The sequence remains: inspect/reserve the real timing host; publish and verify
its reviewed identity; separately authorize physical anchors; publish/verify
anchors; separately authorize N=4/6; later batches on the same frozen environment.

## Next command: inspection only

Fetch and check out the **published commit for this amendment**, activate the
intended Python 3.12 / NumPy 2.3.5 environment, then run from the repository root:

```bash
python3 experiments/a2_larger_deploy_v2/pre_collection.py \
  --checkpoint "$(git rev-parse HEAD)" \
  --cpu 0 \
  --study-output "$HOME/a2-study" \
  --report "$HOME/a2-host-inspection-v2.json"
```

CPU 0 here is an inspection candidate on the reported 0–19 WSL CPU set, not an
approved timing CPU. Use the intended durable study directory if already chosen.
Choose a new report filename if it exists; evidence is never overwritten.
This command performs no solver/reference work and prepares no host freeze.
On WSL, expect a recorded timing-placement block after compatible infrastructure
checks. Send that report for review or inspect a suitable native Linux host.
Do not invoke either collector or the build script at this stage.
