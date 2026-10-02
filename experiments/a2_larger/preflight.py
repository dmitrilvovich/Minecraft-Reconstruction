"""Execute only the separately frozen, <=400-request infrastructure preflight."""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import time

# Keep independent reference work single-threaded too, before NumPy is imported.
os.environ['OPENBLAS_NUM_THREADS']='1'; os.environ['OMP_NUM_THREADS']='1'
from common import (BINARY, MODES, PALETTES, PROTOCOL, ROOT, atomic_json, canonical, digest,
                    file_hash, protocol_identity, read_records, source_identity, verify_archive, write_archive)
from generate import GEOMETRIES, anchors, camera_designs, prepare_subset, structured_designs
from reference import ReferenceBank
from run import AgreementIndex, Worker, call_plan, check_agreement, evaluate_absolute, expand_factors, host_identity

PLAN_PATH=Path(__file__).with_name('preflight-plan.json')
PLAN=json.loads(PLAN_PATH.read_text())

def storage_tests(out):
    out=Path(out); out.mkdir(parents=True,exist_ok=True); rows=[{'record_id':str(i),'value':i} for i in range(3)]
    context={'kind':'preflight_storage_test','binding':'fixed'}
    first=write_archive(out/'first.jsonl.gz',rows,context,3)
    second=write_archive(out/'second.jsonl.gz',rows,context,3)
    if first['sha256']!=second['sha256']: raise AssertionError('Nondeterministic compression')
    def forbidden():
        raise AssertionError('Resume tried to execute valid work')
        yield
    write_archive(out/'first.jsonl.gz',forbidden(),context,3)
    shutil.copyfile(out/'first.jsonl.gz',out/'orphan.jsonl.gz')
    write_archive(out/'orphan.jsonl.gz',forbidden(),context,3)
    damaged=bytearray((out/'first.jsonl.gz').read_bytes()); damaged[len(damaged)//2]^=1
    (out/'corrupt.jsonl.gz').write_bytes(damaged)
    try: verify_archive(out/'corrupt.jsonl.gz')
    except Exception: pass
    else: raise AssertionError('Corrupt archive accepted')
    (out/'interrupted.jsonl.gz.partial.jsonl').write_text('{"incomplete":true}\n')
    try: write_archive(out/'interrupted.jsonl.gz',forbidden(),context,3)
    except RuntimeError: pass
    else: raise AssertionError('Interrupted work silently resumed')
    try: write_archive(out/'first.jsonl.gz',forbidden(),{'binding':'different'},3)
    except ValueError: pass
    else: raise AssertionError('Mismatched identity accepted')
    return dict(valid_job_resume_without_execution=True,sealed_orphan_recovery=True,corrupt_archive_rejected=True,
                interrupted_partial_preserved=True,identity_mismatch_rejected=True,deterministic_compression=True)

def execute(output,cpu):
    out=Path(output); out.mkdir(parents=True,exist_ok=True)
    if (out/'preflight-report.json').exists(): raise RuntimeError('Preflight already complete; inspect it, do not rerun')
    environment=host_identity(); environment['pinned_cpu']=cpu
    if environment['source_subtrees']!=PROTOCOL['freeze']['git_subtrees']: raise ValueError('Accepted source changed')
    context={**protocol_identity(),'source_files':source_identity(),'preflight_plan_sha256':file_hash(PLAN_PATH),
             'environment_sha256':digest(environment),'stage':'preflight_not_collection'}
    atomic_json(out/'host.json',environment); atomic_json(out/'preflight-plan.json',PLAN); atomic_json(out/'identity.json',context)
    wanted={tuple(x) for x in PLAN['camera_pairs']}; robust={tuple(x) for x in PLAN['robustness_pairs']}; designs=[]
    for g in GEOMETRIES.values():
        for c in camera_designs(g):
            key=(c['geometry'],c['k'],c['recipe'],c['density'],c['draw'],c['resolution'],c['suite'])
            if (c['category']=='camera' and key in wanted) or (c['category'],*key) in robust: designs.append(c)
    preparation_start=time.monotonic(); camera_cases,rigs=prepare_subset(designs,out/'inputs')
    chosen={(n,recipe) for n,recipe in PLAN['structured_pairs_identity_canonical']}
    structured=[c for c in structured_designs() if (c['n'],c['recipe']) in chosen and c['permutation']=='identity' and c['factor_order']=='canonical']
    cases=camera_cases+structured+list(anchors())
    if len(cases)!=40: raise AssertionError('Preflight case population differs from plan')
    inputs=write_archive(out/'inputs/cases.jsonl.gz',cases,{**context,'kind':'preflight_inputs'},40)
    atomic_json(out/'inputs/manifest.json',{'cases':inputs,'rigs':rigs,'main_collection':False})
    preparation_seconds=time.monotonic()-preparation_start
    bank=ReferenceBank(out/'inputs',out/'references'); audit_records=[]
    for c in cases:
        before=time.monotonic(); expected=bank.expected(c); constraints=bank.constraints(c,time.monotonic()+300)
        audit_records.append({'record_id':c['record_id'],'n':c['n'],'category':c['category'],'policy':c['reference_policy'],
                              'seconds':time.monotonic()-before,'primitive_tests_per_world':sum(len(h) for h,t in constraints),
                              **expected})
        print(canonical({'preflight_reference':c['record_id'],'n':c['n'],'policy':c['reference_policy']}),flush=True)
    write_archive(out/'reference-checks.jsonl.gz',audit_records,{**context,'kind':'preflight_references'},40)
    requests=0; solver_seconds=0.; results=[]
    def request(worker,c,mode,op,query=None,**kwargs):
        nonlocal requests,solver_seconds
        if requests>=PLAN['maximum_worker_requests'] or solver_seconds>=PLAN['maximum_aggregate_solver_seconds']:
            raise RuntimeError('Deliberately bounded preflight cap reached; no further calls')
        result=worker.call(c,expand_factors(c,out/'inputs'),mode,op,query,**kwargs); requests+=1
        solver_seconds+=(result['ns'] or 0)/1e9
        return result
    def run_rows():
        for ci,c in enumerate(cases):
            worker=Worker(out/'processes'/f'case-{ci:02d}',cpu)
            try:
                for mode in range(4):
                    todo=[('F',None),('P',None)]
                    if c['category']=='camera' and c['geometry']=='ladder04': todo += [('Q',q) for q in c['queries']]
                    for op,q in todo:
                        r=request(worker,c,mode,op,q); validation=bank.validate(c,r,op,q)
                        if r['status']=='UNRESOLVED': raise RuntimeError('Normal preflight call exhausted; pause infrastructure review')
                        row=dict(record_id=digest(['preflight',c['case_id'],mode,op,q]),case_record_id=c['record_id'],
                                 mode=MODES[mode],operation=op,query=q,**r,**validation)
                        results.append(row); yield row
            finally: worker.close()
        check_agreement(results)
    output_archive=write_archive(out/'calls.jsonl.gz',run_rows(),{**context,'kind':'preflight_calls'},368)
    index=AgreementIndex(out/'agreement.sqlite')
    for r in results: index.add(r)
    index.commit(); index.close()
    # Exercise control paths in separate processes; never weaken ordinary worker limits.
    clean=[c for c in cases if c['category']=='camera' and c['geometry']=='ladder04']
    guards=[]
    for i,c in enumerate(clean):
        w=Worker(out/'processes'/f'root-{i}',cpu)
        try:
            r=request(w,c,0,'G'); bank.validate(c,r,'G'); guards.append({'record_id':f'root-{i}','check':'root_diagnostic',**r})
        finally: w.close()
    c=clean[0]
    for op in ('F','P'):
        for reason,override in [('node',{'test_nodes':0}),('time',{'test_seconds':0})]:
            w=Worker(out/'processes'/f'{reason}-{op}',cpu,faults=True)
            try:
                r=request(w,c,0,op,**override)
                if r['status']!='UNRESOLVED' or r['reason']!=reason: raise AssertionError('Budget exhaustion classification')
                guards.append({'record_id':f'{reason}-{op}','check':reason+'_exhaustion',**r})
            finally: w.close()
    for op,reason,options,overrides in [('H','watchdog',{}, {'test_watchdog':.2}),('M','memory',{'memory_bytes':128*1024**2},{}),('X','interrupted_or_missing_output',{}, {})]:
        w=Worker(out/'processes'/reason,cpu,faults=True,**options)
        try:
            r=request(w,c,0,op,**overrides)
            if r['reason']!=reason or r['status']!=('UNRESOLVED' if op!='X' else None): raise AssertionError('External termination classification')
            guards.append({'record_id':reason,'check':reason,**r})
        finally: w.close()
    write_archive(out/'guard-checks.jsonl.gz',guards,{**context,'kind':'preflight_guards'},9)
    storage=storage_tests(out/'storage-tests')
    # Validate scheduling and disagreement protection without executing timing epochs.
    pair=clean; a=[(x[0]['record_id'],x[1],x[2],x[3]) for x in call_plan(pair,0)]
    b=[(x[0]['record_id'],x[1],x[2],x[3]) for x in call_plan(pair,1)]
    if a==b or sorted(map(canonical,a))!=sorted(map(canonical,b)): raise AssertionError('Epoch schedule permutation mismatch')
    altered=copy.deepcopy(results[0]); altered['stats']['search_nodes']+=1
    try: check_agreement([results[0],altered])
    except ValueError: pass
    else: raise AssertionError('Repeated work mismatch undetected')
    report=dict(status='PASS',context=context,design_cases=40,normal_solver_calls=368,root_diagnostics=2,
                fault_budget_calls=4,non_solver_fault_requests=3,total_worker_requests=requests,
                scientific_timing_epochs_executed=0,main_dataset_calls=0,preparation_seconds=preparation_seconds,
                reference_costs=bank.cost,reference_cases=len(audit_records),all_returned_witnesses_checked=True,
                all_four_configurations_checked=True,source_subtrees_unchanged=True,storage_tests=storage,
                scheduling_permutations_verified_without_execution=True,repeat_mismatch_detected=True,
                calls_archive=output_archive,guard_results={r['check']:r['status'] for r in guards},
                result_use='Infrastructure checks only; do not use preflight solver timings/work to tune protocol or as collection observations.')
    atomic_json(out/'preflight-report.json',report)
    print(canonical({'status':'PASS','worker_requests':requests,'main_dataset_calls':0,'timing_epochs':0}),flush=True)
    return report

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('--output',required=True,type=Path); parser.add_argument('--cpu',type=int,required=True)
    args=parser.parse_args(); execute(args.output,args.cpu)
