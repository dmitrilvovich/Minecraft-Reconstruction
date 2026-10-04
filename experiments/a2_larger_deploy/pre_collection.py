"""One nonexperimental bootstrap/check command. This module never starts a solver."""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import sys

from host import (ROOT, SPEC, capture_environment, deployment_identity, frozen,
                  probe_capabilities, resources, restore_binary, verify_repository)
from selection import (DEFAULT_INPUT, dry_run, load_frozen, verify_selection)


def write_new(path, value):
    path = Path(path)
    if path.exists(): raise ValueError('Preserve existing evidence; choose a new output file: ' + str(path))
    frozen.atomic_json(path, value)


def prepare_host_record(report, strategy, asset, reservation):
    if report['status'] != 'PASS': raise ValueError('All nonexperimental checks must pass before preparing a host record')
    if strategy not in ('reserved_physical_host', 'dedicated_non_migrating_vm') or not asset or not reservation:
        raise ValueError('Supply stable reservation strategy, asset identity and reservation evidence')
    return dict(schema='a2-larger-stable-collection-host-v1', status='frozen_for_collection',
                publication_required_before_execution=True, experimental_execution_authorized=False,
                prepared_at_utc=report['checked_at_utc'], prepared_from_checkpoint=report['repository']['checkpoint'],
                stable_execution_strategy=strategy, stable_asset_identity=asset,
                reservation_evidence=reservation, study_output_root=report['resources']['study_output_root'],
                environment=report['environment'], resources_at_preparation=report['resources'],
                capability_checks=report['capabilities'],
                checked_selection_sha256=report['selection']['selection_sha256'],
                deployment_sources=deployment_identity(), solver_calls=0, reference_jobs=0,
                timing_epochs_executed=0)


def check_host(checkpoint, cpu, output, selection_path):
    report = dict(schema='a2-larger-pre-collection-check-v1', status='IN_PROGRESS',
                  checked_at_utc=datetime.now(timezone.utc).isoformat(),
                  purpose='metadata/hash/OS-capability validation only',
                  solver_calls=0, experimental_cases=0, reference_jobs=0, timing_epochs_executed=0,
                  host_frozen_by_this_check=False, checks_completed=[])
    try:
        report['repository'] = verify_repository(checkpoint, selection_path)
        report['checks_completed'].append('exact repository/checkpoint and committed deployment/launch/artifact identities')
        report['binary_bootstrap'] = restore_binary()
        report['checks_completed'].append('compressed and uncompressed frozen binary hashes; no rebuild')
        manifest, cases, locations = load_frozen(DEFAULT_INPUT)
        selection, selected = verify_selection(selection_path, manifest, cases, locations)
        report['selection'] = dict(design_slots=selection['design_slots'], stages=selection['stages'],
                                 selection_sha256=frozen.file_hash(Path(selection_path) / 'selection.json'),
                                 input_manifest_sha256=selection['master_manifest_sha256'],
                                 verified_master_archives=len(manifest['files']) + len(manifest['rigs']),
                                 required_archives=len(selection['required_archives']),
                                 planned_timed_calls=selection['timed_calls'])
        report['checks_completed'].append('original protocol/source/master manifest plus all 40 archive size/hash/decompression records; exact selected IDs')
        report['resources'] = resources(output)
        report['environment'] = capture_environment(cpu, output)
        report['checks_completed'].append('OS/kernel/machine/topology/CPU affinity/runtime symbol versions and library hashes/compiler/RAM/disk/cgroups')
        report['capabilities'] = probe_capabilities(cpu)
        report['checks_completed'].append('synthetic pinned child: 4 GiB RLIMIT_AS, denied oversized mapping, timeout kill/reap; no worker binary')
        report['status'] = 'PASS'
    except Exception as exc:
        report.update(status='BLOCKED', error_type=type(exc).__name__, error=str(exc))
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkpoint', required=True, help='Exact intended published checkout SHA')
    parser.add_argument('--cpu', type=int, required=True)
    parser.add_argument('--study-output', type=Path, required=True, help='Persistent shared output directory for the whole study')
    parser.add_argument('--selection', type=Path, default=ROOT / SPEC['anchor_selection'])
    parser.add_argument('--report', type=Path, required=True, help='New JSON evidence path; existing reports are never overwritten')
    parser.add_argument('--prepare-host-record', type=Path)
    parser.add_argument('--stable-kind', choices=['reserved_physical_host', 'dedicated_non_migrating_vm'])
    parser.add_argument('--asset-id')
    parser.add_argument('--reservation-evidence', help='Public-safe reservation reference or operator attestation; never credentials')
    args = parser.parse_args()
    if args.report.exists(): parser.error('Report already exists; preserve it and choose a new path')
    if args.prepare_host_record and args.prepare_host_record.exists(): parser.error('Host record already exists; do not replace a freeze')
    if args.prepare_host_record and not all((args.stable_kind, args.asset_id, args.reservation_evidence)):
        parser.error('Preparing a host record requires --stable-kind, --asset-id and --reservation-evidence')
    if args.prepare_host_record:
        try: args.prepare_host_record.resolve().relative_to(ROOT)
        except ValueError: parser.error('Host record must be inside this checkout so it can be published')
        if args.prepare_host_record.resolve() == args.report.resolve():
            parser.error('Host record and check report must be separate files')
    args.study_output.mkdir(parents=True, exist_ok=True)
    report = check_host(args.checkpoint, args.cpu, args.study_output, args.selection)
    write_new(args.report, report)
    if report['status'] == 'PASS' and args.prepare_host_record:
        record = prepare_host_record(report, args.stable_kind, args.asset_id, args.reservation_evidence)
        write_new(args.prepare_host_record, record)
    print(frozen.canonical({'status': report['status'], 'report': str(args.report), 'solver_calls': 0,
                            'host_record_prepared': bool(args.prepare_host_record and report['status'] == 'PASS'),
                            'collection_authorized': False}))
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())
