"""Serial process orchestration. Full collection requires an explicit later checkpoint."""
from __future__ import annotations
import argparse
import fcntl
from functools import lru_cache
import hashlib
import json
import math
import mmap
import os
from pathlib import Path
import platform
import resource
import selectors
import sqlite3
import shutil
import struct
import subprocess
import time

from common import (AdministrativeBudget, BINARY, H, MODES, PALETTES, PROTOCOL, ROOT,
                    atomic_json, canonical, digest, file_hash, protocol_identity,
                    read_records, source_identity, verify_archive, write_archive)

def host_identity():
    compiler=Path(shutil.which('g++')).resolve()
    cpuinfo=Path('/proc/cpuinfo').read_text()
    return dict(kernel=platform.uname()._asdict(),python=platform.python_version(),
                compiler=str(compiler),compiler_version=subprocess.check_output(['g++','--version'],text=True).splitlines()[0],
                compiler_sha256=file_hash(compiler),binary_sha256=file_hash(BINARY),
                build_flags='-std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Iinclude',
                allowed_cpus=sorted(os.sched_getaffinity(0)),cpu_model=next((s.split(':',1)[1].strip() for s in cpuinfo.splitlines() if s.startswith('model name')),'unknown'),
                cgroup_memory_max=Path('/sys/fs/cgroup/memory.max').read_text().strip(),
                cgroup_cpu_max=Path('/sys/fs/cgroup/cpu.max').read_text().strip(),
                timing_clock='Linux CLOCK_MONOTONIC via steady_clock; API-start timestamp in shared mmap',
                memory_enforcement='Kernel RLIMIT_AS=4 GiB, caught bad_alloc, plus supervisor RSS polling; peak RSS is worker-level, not solver-only',
                source_subtrees={k:subprocess.check_output(['git','rev-parse','HEAD:'+k],cwd=ROOT,text=True).strip() for k in ('src','include','reference','tests')})

@lru_cache(maxsize=8)
def rig_header(path,sha):
    if file_hash(path)!=sha: raise ValueError('Input rig changed')
    return next(read_records(path))

def expand_factors(case,input_root):
    if 'factors' in case: return case['factors']
    header=rig_header(str(Path(input_root)/case['rig']),case['rig_sha256'])
    return [{'steps':header['factors'][i],'target':target} for i,target in zip(case['factor_indices'],case['labels'])]

class Worker:
    def __init__(self,scratch,cpu,*,faults=False,memory_bytes=None):
        self.scratch=Path(scratch); self.scratch.mkdir(parents=True,exist_ok=True)
        self.inference_lock=(ROOT/'build/a2-larger/inference.lock').open('a')
        fcntl.flock(self.inference_lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        self.control_path=self.scratch/'control.bin'; self.control_path.write_bytes(bytes(24))
        self.control_file=self.control_path.open('r+b'); self.control=mmap.mmap(self.control_file.fileno(),24)
        self.stderr=(self.scratch/'worker.stderr').open('wb'); self.sequence=0; self.cpu=cpu
        limit=memory_bytes or PROTOCOL['budgets']['inference_worker_memory_bytes']
        if memory_bytes is not None and not faults: raise ValueError('Test limit on collection worker')
        def initialize():
            os.sched_setaffinity(0,{cpu}); resource.setrlimit(resource.RLIMIT_AS,(limit,limit)); resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        command=[str(BINARY),'worker',str(self.control_path)] + (['--preflight-faults'] if faults else [])
        self.proc=subprocess.Popen(command,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=self.stderr,preexec_fn=initialize,
                                   env={**os.environ,'OMP_NUM_THREADS':'1','OPENBLAS_NUM_THREADS':'1'})
        self.selector=selectors.DefaultSelector(); self.selector.register(self.proc.stdout,selectors.EVENT_READ)
        self.faults=faults; self.memory_limit=limit; self.peak_rss=0

    def call(self,case,factors,mode,op,query=None,*,test_nodes=None,test_seconds=None,test_watchdog=None):
        if any(x is not None for x in (test_nodes,test_seconds,test_watchdog)) and not self.faults:
            raise ValueError('Preflight overrides prohibited in collection worker')
        self.sequence+=1; nodes=PROTOCOL['budgets']['per_api_search_node_entries'] if test_nodes is None else test_nodes
        seconds=PROTOCOL['budgets']['per_api_seconds'] if test_seconds is None else test_seconds
        watchdog=PROTOCOL['budgets']['external_watchdog_seconds_after_api_start'] if test_watchdog is None else test_watchdog
        q=query or [-1,-1]
        fields=[self.sequence,mode,op,nodes,seconds,PALETTES.index(case['palette']),case['n'],len(factors),*q,*case['domains']]
        for f in factors:
            fields += [f['target'],len(f['steps'])]
            for step in f['steps']: fields += step
        self.proc.stdin.write((' '.join(map(str,fields))+'\n').encode('ascii')); self.proc.stdin.flush()
        submitted=time.monotonic_ns(); buffer=bytearray(); reason=None; start=0
        while True:
            sequence,stamp,ended=struct.unpack('=QQQ',self.control[:24])
            if sequence==self.sequence: start=stamp
            now=time.monotonic_ns()
            if start and not ended and now-start>=watchdog*1e9: reason='watchdog'
            if not start and now-submitted>30e9: reason='setup_timeout'
            try:
                rss=next((int(s.split()[1])*1024 for s in Path(f'/proc/{self.proc.pid}/status').read_text().splitlines() if s.startswith('VmRSS:')),0)
                self.peak_rss=max(self.peak_rss,rss)
                if rss>self.memory_limit: reason='memory'
            except FileNotFoundError: pass
            if reason:
                self.proc.kill(); self.proc.wait(); break
            events=self.selector.select(0.01)
            if events:
                data=os.read(self.proc.stdout.fileno(),1<<20)
                if not data:
                    self.proc.wait(); reason='interrupted_or_missing_output'; break
                buffer.extend(data)
                if b'\n' in buffer:
                    line,extra=buffer.split(b'\n',1)
                    if extra: raise ValueError('Unexpected output after call record')
                    try: result=json.loads(line)
                    except Exception:
                        (self.scratch/f'invalid-output-{self.sequence}.bin').write_bytes(line+b'\n')
                        atomic_json(self.scratch/f'invalid-output-{self.sequence}.json',{'sequence':self.sequence,'operation':op,'case_id':case['case_id'],'start_ns':start or None,'coverage':'invalid','status':None})
                        raise
                    if result['sequence']!=self.sequence: raise ValueError('Worker call sequence mismatch')
                    result.update(coverage='completed' if result['status'] in ('SAT','UNSAT') else 'budget_exhausted' if result['status']=='UNRESOLVED' else 'invalid',
                                  pinned_cpu=self.cpu,worker_pid=self.proc.pid)
                    return result
        known=reason in ('watchdog','memory')
        return dict(sequence=self.sequence,status='UNRESOLVED' if known else None,reason=reason,
                    coverage='budget_exhausted' if known else 'interrupted',technical_error=not known,
                    ns=None,start_ns=start or None,observed_elapsed_ns=time.monotonic_ns()-(start or submitted),
                    stats=None,supported=None,witnesses=[],root=None,worker_peak_rss_bytes=self.peak_rss,
                    pinned_cpu=self.cpu,worker_pid=self.proc.pid)

    def close(self):
        if self.proc.poll() is None:
            self.proc.stdin.close()
            try: self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired: self.proc.kill(); self.proc.wait()
        self.selector.close(); self.proc.stdout.close(); self.stderr.close(); self.control.close(); self.control_file.close()
        if not self.proc.stdin.closed: self.proc.stdin.close()
        self.inference_lock.close()

def call_plan(cases,epoch):
    slots={}
    for c in cases: slots.setdefault(canonical(c['slot_id']),{})[c['palette']]=c
    ordered=sorted(slots,key=lambda s:(H('case_order',epoch,json.loads(s)),s))
    for j,key in enumerate(ordered):
        pair=slots[key]; slot=json.loads(key)
        if set(pair)!=set(PALETTES): raise ValueError('Unpaired case')
        palettes=PALETTES if H('palette_order',epoch,slot)%2==0 else list(reversed(PALETTES))
        modes=list(range(4)); offset=(j+epoch)%4; modes=modes[offset:]+modes[:offset]
        for palette in palettes:
            c=pair[palette]
            for mode in modes:
                for op in (('F','P') if (j+epoch)%2==0 else ('P','F')):
                    yield c,mode,op,None
                for query in c['queries']: yield c,mode,'Q',query

def call_id(c,mode,op,query,epoch):
    return digest([epoch,c['case_id'],MODES[mode],op,query])

def check_agreement(records):
    by_case={}; work={}
    for r in records:
        if r['status'] not in ('SAT','UNSAT'): continue
        key=(r['case_record_id'],r['operation'],canonical(r['query']))
        value=(r['status'],canonical(r.get('supported')))
        if key in by_case and value!=by_case[key]: raise ValueError('Resolved configuration disagreement')
        by_case[key]=value
        wk=(*key,r['mode'])
        if wk in work and r['stats']!=work[wk]: raise ValueError('Completed-repeat work mismatch')
        work[wk]=r['stats']
    for (case,op,q),(status,supports) in by_case.items():
        if op=='F' and (case,'P','null') in by_case and by_case[(case,'P','null')][0]!=status:
            raise ValueError('Feasibility/projection disagreement')
        if op=='Q' and (case,'P','null') in by_case:
            mask=json.loads(by_case[(case,'P','null')][1]); v,s=json.loads(q)
            if mask is not None and ('SAT' if mask[v]&(1<<s) else 'UNSAT')!=status:
                raise ValueError('Direct query/projection disagreement')

class AgreementIndex:
    """Bounded-memory derived index; original records remain in sealed job archives."""
    def __init__(self,path):
        self.db=sqlite3.connect(path)
        self.db.execute('CREATE TABLE IF NOT EXISTS answers (case_id TEXT, op TEXT, query TEXT, value TEXT, PRIMARY KEY(case_id,op,query))')
        self.db.execute('CREATE TABLE IF NOT EXISTS work (case_id TEXT, op TEXT, query TEXT, mode TEXT, value TEXT, PRIMARY KEY(case_id,op,query,mode))')

    def add(self,r):
        if r['status'] not in ('SAT','UNSAT'): return
        key=(r['case_record_id'],r['operation'],canonical(r['query']))
        answer=canonical([r['status'],r.get('supported')]); work=canonical(r['stats'])
        old=self.db.execute('SELECT value FROM answers WHERE case_id=? AND op=? AND query=?',key).fetchone()
        if old and old[0]!=answer: raise ValueError('Cross-configuration or repeated answer mismatch')
        self.db.execute('INSERT OR IGNORE INTO answers VALUES (?,?,?,?)',(*key,answer))
        wk=(*key,r['mode']); old=self.db.execute('SELECT value FROM work WHERE case_id=? AND op=? AND query=? AND mode=?',wk).fetchone()
        if old and old[0]!=work: raise ValueError('Completed-repeat integer-work mismatch')
        self.db.execute('INSERT OR IGNORE INTO work VALUES (?,?,?,?,?)',(*wk,work))
        answers=self.db.execute('SELECT op,query,value FROM answers WHERE case_id=?',(key[0],)).fetchall()
        values={(op,q):json.loads(v) for op,q,v in answers}
        if ('F','null') in values and ('P','null') in values and values['F','null'][0]!=values['P','null'][0]:
            raise ValueError('Feasibility/projection mismatch')
        projection=values.get(('P','null'))
        if projection:
            for (op,q),(status,_) in values.items():
                if op=='Q':
                    v,s=json.loads(q)
                    if status!=('SAT' if projection[1][v]&(1<<s) else 'UNSAT'): raise ValueError('Query/projection mismatch')

    def commit(self): self.db.commit()

    def close(self): self.db.close()

def operating(case):
    return (case['category']=='camera' and case['geometry'].startswith('ladder') and case['density']=='half'
            and case['k'] in (case['n']//2,case['n']) and case['suite'] in ('axis_2','axis_3') and 'nominal_density' in case['regimes'])

def percentile(values,p=.95):
    values=sorted(values); return values[max(0,math.ceil(len(values)*p)-1)]

def evaluate_absolute(cases,records,expected_epochs):
    """Finite-design engineering screens only; called at declared epoch boundaries."""
    indexed={c['record_id']:c for c in cases}; grouped={}; reasons=[]
    for r in records:
        c=indexed[r['case_record_id']]
        if operating(c) and r['mode']=='both' and r['operation'] in ('F','P'):
            key=(c['n'],c['k'],c['suite'],c['palette'],r['operation'])
            grouped.setdefault(key,{}).setdefault(c['record_id'],[]).append(r)
        if r.get('worker_peak_rss_bytes',0)>2*1024**3: reasons.append('worker_peak_above_2GiB')
    summaries={}
    for key,slots in grouped.items():
        failure=sum(any(r['status']=='UNRESOLVED' for r in rs) for rs in slots.values())/len(slots)
        complete=all(len(rs)==expected_epochs and all(r['status'] in ('SAT','UNSAT') for r in rs) for rs in slots.values())
        if failure>=.05: reasons.append(str(key)+': unresolved_at_least_5_percent')
        out={'unresolved_rate':failure,'complete':complete}
        if complete:
            median=lambda x: sorted(x)[len(x)//2]
            times=[median([r['ns']/1e9 for r in rs]) for rs in slots.values()]
            nodes=[median([r['stats']['search_nodes'] for r in rs]) for rs in slots.values()]
            out.update(p95_seconds=percentile(times),p95_nodes=percentile(nodes),max_nodes=max(nodes))
            if out['p95_seconds']>(5 if key[-1]=='P' else 1): reasons.append(str(key)+': absolute_time')
            if out['p95_nodes']>=100000: reasons.append(str(key)+': absolute_nodes')
        summaries[canonical(key)]=out
    return {'pause_reasons':sorted(set(reasons)),'operating_summaries':summaries}

def evaluate_growth(summaries,records):
    reasons=[]
    for key,current in summaries.items():
        n,k,suite,palette,op=json.loads(key)
        if n!=12 or not current['complete']: continue
        old=summaries.get(canonical([8,4 if k==6 else 8,suite,palette,op]))
        if not old or not old['complete']: continue
        if current['p95_nodes']>=10000 and current['p95_nodes']/max(1,old['p95_nodes'])>8: reasons.append(key+': node_growth')
        threshold=1 if op=='P' else .1
        if current['p95_seconds']>=threshold and old['p95_seconds']>0 and current['p95_seconds']/old['p95_seconds']>8: reasons.append(key+': time_growth')
    peaks={}
    for r in records:
        job=r.get('job_id',[])
        if len(job)!=4 or job[1] not in ('ladder08','ladder12'): continue
        n=8 if job[1]=='ladder08' else 12
        if job[2]!=8*(4 if n==8 else 6): continue
        key=(n,job[0],job[3]); peaks[key]=max(peaks.get(key,0),r.get('worker_peak_rss_bytes',0))
    for (n,category,suite),peak in peaks.items():
        old=peaks.get((8,category,suite),0)
        if n==12 and old and peak>512*1024**2 and peak/old>4: reasons.append(str((category,suite))+': memory_growth')
    return sorted(set(reasons))

def collect(input_root,output,cpu,authorization):
    """Not invoked by preflight. Requires published input evidence and a future authorization ID."""
    from reference import ReferenceBank
    input_root=Path(input_root); output=Path(output); output.mkdir(parents=True,exist_ok=True)
    if not authorization or len(authorization)!=40: raise RuntimeError('Later collection checkpoint hash required')
    lock=(output/'collection.lock').open('a'); fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    manifest=json.loads((input_root/'input-manifest.json').read_text()); identity=protocol_identity()
    if not manifest['complete'] or manifest['design_slots']!=72148: raise ValueError('Incomplete input population')
    if any(manifest[k]!=v for k,v in identity.items()): raise ValueError('Protocol/binary differs from frozen inputs')
    if manifest['source_files']!=source_identity(): raise ValueError('Collector/reference source changed after freeze')
    environment=host_identity(); environment.update(pinned_cpu=cpu)
    if environment['source_subtrees']!=PROTOCOL['freeze']['git_subtrees']: raise ValueError('Accepted source subtree changed')
    identity.update(input_manifest_sha256=file_hash(input_root/'input-manifest.json'),environment_sha256=digest(environment),authorization_checkpoint=authorization)
    old=output/'collection-identity.json'
    if old.exists() and json.loads(old.read_text())!=identity: raise ValueError('Resume identity differs')
    atomic_json(old,identity); atomic_json(output/'host.json',environment)
    ledger=AdministrativeBudget(output/'administrative-costs.json'); ledger.reserve(output)
    if not (output/'input-preparation-accounted.json').exists():
        receipt=json.loads((input_root/'preparation-receipt.json').read_text())
        ledger.add('preparation',receipt['seconds']); atomic_json(output/'input-preparation-accounted.json',receipt)
    initial_start=time.monotonic(); cases=[]
    for entry in manifest['rigs']:
        verify_archive(input_root/entry['path'],{k:v for k,v in entry.items() if k!='path'})
    for entry in manifest['files']:
        desc={k:v for k,v in entry.items() if k!='path'}; verify_archive(input_root/entry['path'],desc)
        cases.extend(read_records(input_root/entry['path']))
    ledger.add('preparation',time.monotonic()-initial_start)
    jobs={}
    for c in cases: jobs.setdefault(canonical(c['job_id']),[]).append(c)
    inventories={canonical(x['job_id']):x for x in manifest['jobs']}
    index=AgreementIndex(output/'agreement.sqlite'); all_summaries={}; memory_records=[]; total_records=0
    indexed_cases={c['record_id']:c for c in cases}
    # Anchors first, then each size's five epochs; no concurrent solver jobs.
    stages=[('anchors',8)]+[('volume',n) for n in (4,6,8,10,12)]
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
    if total_records!=manifest['timed_calls']: raise ValueError('Final call count differs from frozen population')
    index.close()
    atomic_json(output/'collection-complete.json',{'identity':identity,'timed_calls':total_records,'scientific_review_pending':True})

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,required=True); parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--cpu',type=int,required=True); parser.add_argument('--collection-authorized-checkpoint',required=True)
    args=parser.parse_args(); collect(args.input,args.output,args.cpu,args.collection_authorized_checkpoint)
