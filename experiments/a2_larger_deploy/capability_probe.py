"""Synthetic OS-capability child; no experimental IDs, inputs or worker binary."""
import argparse
import json
import mmap
import os
import resource
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('mode', choices=['limits', 'watchdog'])
parser.add_argument('--cpu', type=int, required=True)
args = parser.parse_args()
os.sched_setaffinity(0, {args.cpu})
if os.sched_getaffinity(0) != {args.cpu}:
    raise RuntimeError('Affinity did not take effect')
if args.mode == 'limits':
    ceiling = 4 * 1024**3
    resource.setrlimit(resource.RLIMIT_AS, (ceiling, ceiling))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    # An anonymous reservation at least as large as the whole process ceiling
    # must fail. No pages are touched and no large resident allocation occurs.
    rejected = False
    try:
        allocation = mmap.mmap(-1, ceiling)
    except (OSError, MemoryError):
        rejected = True
    else:
        allocation.close()
    if not rejected:
        raise RuntimeError('Address-space ceiling was not enforced')
    print(json.dumps({'kind': 'infrastructure_only', 'rlimit_as': list(resource.getrlimit(resource.RLIMIT_AS)),
                      'rlimit_core': list(resource.getrlimit(resource.RLIMIT_CORE)),
                      'oversized_virtual_mapping_rejected': rejected,
                      'pinned_cpus': sorted(os.sched_getaffinity(0))}), flush=True)
else:
    print(json.dumps({'kind': 'infrastructure_only', 'ready': True, 'clock': 'CLOCK_MONOTONIC'}), flush=True)
    # The parent must kill this waiting child using its short validation timeout.
    time.sleep(5)
    raise RuntimeError('Synthetic watchdog did not terminate the child')
