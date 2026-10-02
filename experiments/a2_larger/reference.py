"""Independent exact AABB/finite-world and graph references; no production automaton.

Only the frozen Python primitive intersection is imported. Its propagation/search
functions are never called. Geometry is reconstructed from serialized rational rays.
"""
from __future__ import annotations
from fractions import Fraction as Q
import importlib.util
import itertools
import json
from pathlib import Path
import sys
import time
import numpy as np

from common import ROOT, canonical, digest, file_hash, read_records, verify_archive, write_archive

FROZEN=ROOT/'reference/python/phase_a.py'
spec=importlib.util.spec_from_file_location('_mcr_frozen_aabb',FROZEN)
frozen=importlib.util.module_from_spec(spec); sys.modules[spec.name]=frozen; spec.loader.exec_module(frozen)

def check_deadline(deadline):
    if time.monotonic()>=deadline:
        raise TimeoutError('Mandatory reference task exceeded its deadline; collection must pause')

def primitive_hits(ray,shape):
    origin=tuple(Q(*v) for v in ray['origin']); direction=tuple(Q(*v) for v in ray['direction']); hits=[]
    for v,pos in enumerate(itertools.product(*(range(d) for d in shape))):
        for state in (1,2):
            lo=tuple(Q(pos[j])+(Q(1,2) if state==2 and j==1 else 0) for j in range(3))
            hi=tuple(Q(pos[j])+(Q(1,2) if state==1 and j==1 else 1) for j in range(3))
            depth=frozen.oracle_intersection(origin,direction,lo,hi)
            if depth is not None: hits.append((depth,v,state))
    return tuple((v,s) for _,v,s in sorted(hits))

def color(state,palette):
    # Explicit C++ observation codes: background=0, stone=1, oak=2.
    return 2 if state==1 or palette=='same_material' else 1

def scalar_image(hits,world,palette):
    for v,s in hits:
        if world[v]==s: return color(s,palette)
    return 0

def scalar_accept(constraints,world,palette):
    return all(scalar_image(hits,world,palette)==target for hits,target in constraints)

def factor_constraints(case):
    return [(tuple((v,s) for v,b,t in f['steps'] for s,hit in ((1,b),(2,t)) if hit),f['target']) for f in case['factors']]

def enumerate_reference(case,constraints,deadline):
    values=[[s for s in range(3) if m&(1<<s)] for m in case['domains']]
    count=1
    for vs in values: count*=len(vs)
    supports=[0]*case['n']; family=0
    for start in range(0,count,65536):
        check_deadline(deadline)
        codes=np.arange(start,min(start+65536,count),dtype=np.uint64)
        worlds=np.empty((len(codes),case['n']),dtype=np.uint8)
        for v in reversed(range(case['n'])):
            worlds[:,v]=np.asarray(values[v],dtype=np.uint8)[codes%len(values[v])]; codes//=len(values[v])
        for hits,target in constraints:
            if not len(worlds): break
            output=np.zeros(len(worlds),dtype=np.uint8)
            for v,s in hits:
                selected=(output==0)&(worlds[:,v]==s)
                output[selected]=color(s,case['palette'])
            worlds=worlds[output==target]
        family+=len(worlds)
        for v in range(case['n']):
            for s in np.unique(worlds[:,v]): supports[v]|=1<<int(s)
    check_deadline(deadline)
    return {'status':'SAT' if family else 'UNSAT','supports':supports,'family_size':family,'enumerated_worlds':count}

def graph_reference(case):
    """Independent parity proof plus binary fixed-region counting, no ray DP."""
    n=case['n']; adj=[[] for _ in range(n)]; active=set(); fixed=set(case['fixed_vertices'])
    for a,b in case['edges']: adj[a].append(b); adj[b].append(a); active.update((a,b))
    colors={}; components=[]
    for v in sorted(active):
        if v in colors: continue
        colors[v]=0; queue=[v]; component=[]
        while queue:
            a=queue.pop(); component.append(a)
            for b in adj[a]:
                if b in colors and colors[b]==colors[a]:
                    return {'status':'UNSAT','supports':[0]*n,'family_size':0,'proof':'odd_cycle'}
                if b not in colors: colors[b]=1-colors[a]; queue.append(b)
        components.append(component)
    supports=[0]*n; family=1
    for comp in components:
        orientations=[0,1]; anchor=case['anchor_vertex']
        if anchor in comp: orientations=[colors[anchor]]
        valid=[o for o in orientations if all(case['domains'][v]&(1<<(1+(colors[v]^o))) for v in comp)]
        family*=len(valid)
        for o in valid:
            for v in comp: supports[v]|=1<<(1+(colors[v]^o))
    if fixed:
        ways=0; masks={v:0 for v in fixed}
        for choices in itertools.product((0,1),repeat=len(fixed)):
            if not any(choices): continue
            if all(case['domains'][v]&(1<<s) for v,s in zip(sorted(fixed),choices)):
                ways+=1
                for v,s in zip(sorted(fixed),choices): masks[v]|=1<<s
        family*=ways
        for v in fixed: supports[v]=masks[v]
    if active|fixed!=set(range(n)) or active&fixed:
        raise ValueError('Graph reference does not cover these vertices independently')
    if not family: supports=[0]*n
    return {'status':'SAT' if family else 'UNSAT','supports':supports,'family_size':family,'proof':'parity_components_and_fixed_hit'}

class ReferenceBank:
    def __init__(self,input_root,cache_root):
        self.input_root=Path(input_root); self.cache_root=Path(cache_root); self.rigs={}; self.case_constraints={}
        self.exact={}; self.checked_witnesses=set(); self.cost={'geometry_seconds':0.,'reference_seconds':0.,'witness_seconds':0.,'witness_requests':0,'unique_witness_checks':0}
        self.identity={'adapter_sha256':file_hash(Path(__file__)),'frozen_reference_sha256':file_hash(FROZEN)}

    def rig(self,case,deadline):
        key=case['rig_sha256']
        if key in self.rigs: return self.rigs[key]
        start=time.monotonic(); source=self.input_root/case['rig']
        if file_hash(source)!=key: raise ValueError('Raw rig hash changed')
        path=self.cache_root/'geometry'/f'{key}.jsonl.gz'; context={**self.identity,'rig_sha256':key}
        if not path.exists():
            def rows():
                shape=None
                for raw in read_records(source):
                    check_deadline(deadline)
                    if raw['kind']=='rig': shape=raw['shape']; yield {'kind':'reference_rig','shape':shape,'resolution':raw['resolution']}
                    else:
                        if shape is None: raise ValueError('Rig ordering')
                        yield {'kind':'ray','index':raw['index'],'camera':raw['camera'],'factor':raw['factor'],
                               'hits':primitive_hits(raw,shape)}
            write_archive(path,rows(),context)
        verify_archive(path,expected_context=context)
        records=list(read_records(path)); self.rigs[key]=records
        self.cost['geometry_seconds']+=time.monotonic()-start
        return records

    def constraints(self,case,deadline):
        key=digest(case)
        if key in self.case_constraints: return self.case_constraints[key]
        if case['category']=='structured': result=factor_constraints(case)
        elif case['category']=='anchor': result=[(primitive_hits(r,case['shape']),r['target']) for r in case['rays']]
        else:
            data=self.rig(case,deadline); selected=set(case['camera_indices']); combined=set()
            base_count=len(case['factor_indices'])-(case['category']=='contradictory')
            labels=dict(zip(case['factor_indices'][:base_count],case['labels'][:base_count]))
            for r in data[1:]:
                if r['camera'] in selected: combined.add((tuple(tuple(x) for x in r['hits']),labels[r['factor']]))
                if r['index']==case.get('duplicate_ray_index'):
                    combined.add((tuple(tuple(x) for x in r['hits']),case['labels'][-1]))
            result=sorted(combined)
        self.case_constraints[key]=result
        return result

    def expected(self,case,seconds=300):
        key=digest(case)
        if key in self.exact: return self.exact[key]
        start=time.monotonic(); geometry_before=self.cost['geometry_seconds']; deadline=start+seconds; policy=case['reference_policy']
        path=self.cache_root/'cases'/f'{key}.jsonl.gz'; context={**self.identity,'case_sha256':key}
        if path.exists():
            verify_archive(path,expected_context=context); answer=list(read_records(path))[0]
        else:
            constraints=self.constraints(case,deadline)
            if case['category']=='structured':
                answer=graph_reference(case)
                if case['exhaustive_crosscheck']:
                    brute=enumerate_reference(case,constraints,deadline)
                    for field in ('status','supports','family_size'):
                        if brute[field]!=answer[field]: raise ValueError('Structured reference disagreement')
                    answer['enumerated_worlds']=brute['enumerated_worlds']
            elif policy=='exhaustive': answer=enumerate_reference(case,constraints,deadline)
            elif policy=='contradiction_proof':
                by_hits={}
                for hits,target in constraints: by_hits.setdefault(hits,set()).add(target)
                if not any(len(v)>1 for v in by_hits.values()): raise ValueError('Missing independent contradictory observations')
                answer={'status':'UNSAT','supports':[0]*case['n'],'family_size':0,'proof':'contradictory_duplicate'}
            else: answer={'status':None,'supports':None,'family_size':None}
            if case.get('truth') is not None and case['category']!='contradictory':
                if not scalar_accept(constraints,case['truth'],case['palette']): raise ValueError('Generating truth fails independent raw-ray reference')
            if answer['status'] is not None and answer['status']!=case['expected_status']:
                raise ValueError('Declared construction/reference disagreement')
            answer.update(reference_type=policy,exact=answer['supports'] is not None,case_sha256=key)
            check_deadline(deadline)
            write_archive(path,[answer],context,1)
        self.exact[key]=answer; self.cost['reference_seconds']+=time.monotonic()-start-(self.cost['geometry_seconds']-geometry_before)
        return answer

    def validate(self,case,result,operation,query=None):
        expected=self.expected(case); start=time.monotonic()
        if result.get('technical_error'): raise ValueError('Worker technical failure: '+result.get('reason',''))
        if result['status']=='UNRESOLVED':
            if result.get('supported') is not None or result.get('witnesses'): raise ValueError('Exhausted call exposed certified results')
            return {'reference_type':expected['reference_type'],'reference_complete':expected['exact'],'witness_checked':False}
        if result['status'] not in ('SAT','UNSAT'): raise ValueError('Missing scientific outcome')
        witnesses=result['witnesses']; supported=result.get('supported'); constraints=self.constraints(case,time.monotonic()+300)
        key=digest(case); union=[0]*case['n']
        for w in witnesses:
            self.cost['witness_requests']+=1
            if len(w)!=case['n'] or any(s not in (0,1,2) or not case['domains'][v]&(1<<s) for v,s in enumerate(w)):
                raise ValueError('Witness outside original domains')
            if operation=='Q' and w[query[0]]!=query[1]: raise ValueError('Witness violates requested literal')
            cache_key=(key,tuple(w))
            if cache_key not in self.checked_witnesses:
                if not scalar_accept(constraints,w,case['palette']): raise ValueError('Witness fails independent original observations')
                self.checked_witnesses.add(cache_key); self.cost['unique_witness_checks']+=1
            for v,s in enumerate(w): union[v]|=1<<s
        if operation=='G':
            if expected['exact']:
                root=result['root']
                if not root['consistent'] and expected['status']=='SAT': raise ValueError('Unsound root GAC')
                if any(a&b!=b for a,b in zip(root['domains'],expected['supports'])): raise ValueError('Unsound root pruning')
        else:
            if result['status']=='SAT' and not witnesses: raise ValueError('Missing SAT witness')
            if result['status']=='UNSAT' and witnesses: raise ValueError('UNSAT has witness')
            if operation=='P' and (supported!=union or len(supported)!=case['n']): raise ValueError('Projection witness coverage differs')
            status=expected['status']
            if operation=='Q' and expected['exact']: status='SAT' if expected['supports'][query[0]]&(1<<query[1]) else 'UNSAT'
            if operation=='Q' and not expected['exact'] and case.get('truth') is not None and case['category']!='contradictory' and case['truth'][query[0]]==query[1]: status='SAT'
            if operation!='Q' and status is None: status=case['expected_status']
            if status is not None and status!=result['status']: raise ValueError('Answer/reference disagreement')
            if operation=='P' and expected['exact'] and supported!=expected['supports']: raise ValueError('Complete projection/reference disagreement')
        self.cost['witness_seconds']+=time.monotonic()-start
        return {'reference_type':expected['reference_type'],'reference_complete':expected['exact'],
                'reference_sha256':digest(expected),'witness_checked':True,'family_size':expected['family_size']}
