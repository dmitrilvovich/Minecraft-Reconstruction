"""Scoped orchestration v1: the frozen orchestration body with explicit batch guards.

No execution occurs on import. Do not use the legacy CLI for partial batches.
All unselected earlier stages must already exist as verified sealed evidence.
"""
from __future__ import annotations
import argparse
import fcntl
import hashlib
import json
from pathlib import Path
import time

from selection import (AMENDMENT, DEFAULT_INPUT, MASTER_SHA256, Scope, frozen,
                       implementation_identity, legacy, load_frozen, verify_selection)
from guards import (ScopedWorker, bind_batch, check_prerequisites,
                    require_committed, require_environment)
from common import (AdministrativeBudget, H, MODES, atomic_json, canonical, digest,
                    file_hash, protocol_identity, read_records, write_archive)
from run import (AgreementIndex, call_id, call_plan, check_agreement,
                 evaluate_absolute, evaluate_growth, expand_factors, operating)

def collect(input_root,output,cpu,authorization,selection_path,environment_path):
    """Not invoked by preflight. Requires published input evidence and a future authorization ID."""
    from reference import ReferenceBank
    started = time.monotonic()
    input_root=Path(input_root); output=Path(output)
    environment, environment_document = require_environment(environment_path, cpu, authorization)
    require_committed(Path(selection_path)/'selection.json', authorization)
    require_committed(Path(selection_path)/'selected-design-ids.jsonl.gz', authorization)
    manifest, cases, locations = load_frozen(input_root)
    selection, selected = verify_selection(selection_path, manifest, cases, locations)
    scope = Scope(selection, selected)
    identity = protocol_identity()
    identity.update(input_manifest_sha256=MASTER_SHA256,
                    environment_sha256=digest(environment),
                    collection_environment_record_sha256=file_hash(environment_path),
                    amendment_sha256=file_hash(AMENDMENT),
                    launch_implementation_sha256=digest(implementation_identity()))
    output.mkdir(parents=True,exist_ok=True)
    lock=(output/'collection.lock').open('a'); fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    old=output/'collection-identity.json'
    if old.exists() and json.loads(old.read_text())!=identity: raise ValueError('Resume identity differs')
    stages = check_prerequisites(output, identity, selection, cases)
    # This study directory is shared across batches: never reset its cost ledger,
    # references, raw jobs, repeat-work index or engineering screens per subset.
    if (output/'jobs').exists() and not old.exists():
        raise ValueError('Existing study jobs without a collection identity')
    batch_directory = bind_batch(output, selection)
    atomic_json(old,identity); atomic_json(output/'host.json',environment)
    atomic_json(batch_directory/'authorization.json', {'checkpoint':authorization,
                'selection_sha256':digest(selection), 'identity':identity})
    ledger=AdministrativeBudget(output/'administrative-costs.json'); ledger.reserve(output)
    ledger.add('solver',0.); ledger.add('preparation',time.monotonic()-started)
    if not (output/'input-preparation-accounted.json').exists():
        receipt=json.loads((input_root/'preparation-receipt.json').read_text())
        ledger.add('preparation',receipt['seconds']); atomic_json(output/'input-preparation-accounted.json',receipt)
    def Worker(scratch, selected_cpu):
        require_environment(environment_path, selected_cpu, authorization)
        ledger.reserve(output)
        return ScopedWorker(scratch, selected_cpu, scope)
    jobs={}
    for c in cases: jobs.setdefault(canonical(c['job_id']),[]).append(c)
    inventories={canonical(x['job_id']):x for x in manifest['jobs']}
    index=AgreementIndex(output/'agreement.sqlite'); all_summaries={}; memory_records=[]; total_records=0
    indexed_cases={c['record_id']:c for c in cases}
    # Anchors first, then each size's five epochs; no concurrent solver jobs.
    selected_records=0
    for stage,n in stages:
        keys=[key for key,cs in jobs.items() if cs[0]['n']==n and (cs[0]['category']=='anchor')==(stage=='anchors')]
        stage_records=[]; stage_cases=[c for key in keys for c in jobs[key]]; stage_peaks=[]
        for epoch in range(5):
            for key in sorted(keys,key=lambda k:(H('job_order',epoch,json.loads(k)),k)):
                cs=jobs[key]; job_id=json.loads(key); job_hash=digest([epoch,job_id]); path=output/'jobs'/f'{job_hash}.jsonl.gz'
                expected_keys=hashlib.sha256()
                for c,mode,op,query in call_plan(cs,epoch):
                    expected_keys.update(canonical(call_id(c,mode,op,query,epoch)).encode('ascii')+b'\n')
                context={**identity,'epoch':epoch,'job_id':job_id,'expected_calls':inventories[key]['calls_per_epoch'],
                         'expected_keys_sha256':expected_keys.hexdigest()}
                meter_start=time.monotonic(); meter_solver=0.
                def account_preparation():
                    nonlocal meter_start,meter_solver
                    elapsed=time.monotonic()-meter_start
                    ledger.add('preparation',max(0.,elapsed-meter_solver)); meter_start=time.monotonic(); meter_solver=0.
                def rows():
                    nonlocal meter_solver
                    if key not in scope.jobs:
                        raise RuntimeError('Out-of-batch stage may only be read from sealed evidence')
                    bank=ReferenceBank(input_root,output/'reference')
                    for c in cs:
                        bank.expected(c); account_preparation()
                    # Root diagnostics have their own fresh process and archive, never reused domains.
                    root_path=output/'roots'/f'{digest(job_id)}.jsonl.gz'
                    def roots():
                        worker=Worker(output/'processes'/('root-'+digest(job_id)),cpu)
                        try:
                            for c in cs:
                                result=worker.call(c,expand_factors(c,input_root),0,'G')
                                if result['status']=='UNRESOLVED': raise RuntimeError('Untimed root diagnostic exhausted; pause')
                                validation=bank.validate(c,result,'G')
                                yield dict(record_id=c['record_id'],case_record_id=c['record_id'],**result,**validation)
                                account_preparation()
                        finally: worker.close()
                    write_archive(root_path,roots(),{**identity,'job_id':job_id,'kind':'root_diagnostics'},len(cs))
                    worker=Worker(output/'processes'/job_hash,cpu); results=[]
                    try:
                        for c,mode,op,query in call_plan(cs,epoch):
                            result=worker.call(c,expand_factors(c,input_root),mode,op,query)
                            measured=result['ns'] if result['ns'] is not None else result['observed_elapsed_ns']
                            meter_solver+=measured/1e9
                            error=None
                            try: validation=bank.validate(c,result,op,query)
                            except Exception as exc:
                                error=exc; validation={'validation_error':str(exc),'reference_complete':False,'witness_checked':False,
                                                      'raw_worker_status':result['status'],'status':None,'technical_error':True,'coverage':'invalid'}
                            row=dict(record_id=call_id(c,mode,op,query,epoch),case_record_id=c['record_id'],job_id=job_id,
                                     epoch=epoch,mode=MODES[mode],operation=op,query=query,**result,**validation)
                            results.append(row); yield row
                            ledger.add('solver',measured/1e9); account_preparation()
                            if error: raise error
                            if worker.proc.poll() is not None: raise RuntimeError('Worker terminated; remaining coverage unattempted, review required')
                        check_agreement(results)
                    finally: worker.close()
                desc=write_archive(path,rows(),context,inventories[key]['calls_per_epoch'])
                peak=0
                for r in read_records(path):
                    if key in scope.jobs:
                        scope.check_record(r); selected_records+=1
                    index.add(r); total_records+=1; peak=max(peak,r['worker_peak_rss_bytes'])
                    if operating(indexed_cases[r['case_record_id']]) and r['mode']=='both':
                        stage_records.append({k:v for k,v in r.items() if k not in ('witnesses','supported','root')})
                index.commit(); memory_records.append({'job_id':job_id,'worker_peak_rss_bytes':peak})
                stage_peaks.append(peak)
                atomic_json(output/'completed-jobs'/f'{job_hash}.json',desc)
                account_preparation()
            if stage=='volume' and epoch in (0,4):
                screen=evaluate_absolute(stage_cases,stage_records,epoch+1)
                if any(peak>2*1024**3 for peak in stage_peaks): screen['pause_reasons'].append('worker_peak_above_2GiB')
                atomic_json(output/'screens'/f'n{n}-epochs{epoch+1}.json',screen)
                if screen['pause_reasons']: raise RuntimeError('Preregistered engineering pause: '+canonical(screen['pause_reasons']))
                if epoch==4: all_summaries.update(screen['operating_summaries'])
        if stage=='volume' and n==12:
            growth=evaluate_growth(all_summaries,memory_records); atomic_json(output/'growth-screen.json',{'pause_reasons':growth})
            if growth: raise RuntimeError('Preregistered growth review required')
    if selected_records!=selection['timed_calls']: raise ValueError('Final call count differs from frozen subset')
    index.close()
    atomic_json(batch_directory/'batch-complete.json',{'identity':identity,'selection_sha256':digest(selection),
                'design_slots':selection['design_slots'],'timed_calls':selected_records,
                'scientific_review_pending':True})
    if total_records==manifest['timed_calls']:
        atomic_json(output/'collection-complete.json',{'identity':identity,'timed_calls':total_records,'scientific_review_pending':True})

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,default=DEFAULT_INPUT)
    parser.add_argument('--output',type=Path,required=True,help='One shared study directory across ALL batches')
    parser.add_argument('--selection',type=Path,required=True)
    parser.add_argument('--collection-environment',type=Path,required=True)
    parser.add_argument('--cpu',type=int,required=True)
    parser.add_argument('--collection-authorized-checkpoint',required=True)
    args=parser.parse_args()
    collect(args.input,args.output,args.cpu,args.collection_authorized_checkpoint,
            args.selection,args.collection_environment)
