"""Deterministic input preparation. This module never calls an inference API."""
from __future__ import annotations
import argparse
from collections import deque
import itertools
import json
from pathlib import Path
import subprocess
import time

from common import (BINARY, H, MODES, PALETTES, PROTOCOL, ROOT, atomic_json, canonical,
                    digest, file_hash, protocol_identity, read_records, source_identity, verify_archive, write_archive)

GEOMETRIES = {g['id']:g for g in PROTOCOL['geometries']}
SUITES = {s['id']:s['camera_indices'] for s in PROTOCOL['cameras']['suites']}

def mask_orders(g):
    nx,ny,nz = g['shape_xyz']; n=g['n']; gid=g['id']
    positions = list(itertools.product(range(nx),range(ny),range(nz)))
    distance = lambda a,b: sum(abs(x-y) for x,y in zip(positions[a],positions[b]))
    anchor = min(range(n),key=lambda v:(H('mask_anchor',gid,v),v))
    queue=deque([anchor]); seen={anchor}; compact=[]
    while queue:
        v=queue.popleft(); compact.append(v)
        for u in range(n):
            if u not in seen and distance(u,v)==1:
                seen.add(u); queue.append(u)
    spread=[anchor]
    while len(spread)<n:
        spread.append(min((v for v in range(n) if v not in spread),
                          key=lambda v:(-min(distance(v,u) for u in spread),v)))
    return {'compact':compact,'spread':spread,
            **{'random'+str(i):sorted(range(n),key=lambda v:(H('mask_random',gid,i,v),v)) for i in (0,1)}}

def queries(identity,n):
    cells=sorted(range(n),key=lambda v:(H('query_cell',identity,v),v))[:2]
    return [[v,s] for v in sorted(cells) for s in range(3)]

def audit_camera(c):
    if c['n']<=8:
        return 'exhaustive'
    if c['category']=='contradictory':
        return 'contradiction_proof'
    if 'nominal_density' not in c['regimes'] or c['draw']!=0:
        return 'witness_only'
    recipe='endpoint' if c['k'] in (0,c['n']) else 'compact'
    if c['recipe']!=recipe:
        return 'witness_only'
    if c['category']=='camera' and c['suite']=={'low':'axis_1','half':'axis_6','high':'oblique_2'}[c['density']]:
        return 'exhaustive'
    if c['category']=='restricted' and c['k']==c['n']//2 and c['density']=='half' and c['suite']=='axis_6':
        return 'exhaustive'
    return 'witness_only'

def camera_designs(g):
    n=g['n']; gid=g['id']; orders=mask_orders(g); L=max(g['shape_xyz'])
    for k in g['k']:
        recipes=['endpoint'] if k in (0,n) else PROTOCOL['determinism']['intermediate_mask_recipes']
        for recipe in recipes:
            selected=list(range(n)) if k==n else [] if k==0 else sorted(orders[recipe][:k])
            base_domains=[7 if v in selected else 3 for v in range(n)]
            for target in PROTOCOL['camera_truths']['occupancy_targets']:
                density=target['id']; a,b=target['a'],target['b']
                for draw in PROTOCOL['camera_truths']['draw_indices']:
                    q=n*a//b+int(draw%b<(n*a)%b)
                    occupied=set(sorted(range(n),key=lambda v:(H('occupancy',gid,draw,v),v))[:q])
                    truth=[0 if v not in occupied else (1+H('shape',gid,draw,v)%2 if v in selected else 1) for v in range(n)]
                    for suite in SUITES:
                        resolutions={}
                        for r,regime in [(8*L,'nominal_density'),(16,'fixed_image')]:
                            resolutions.setdefault(r,[]).append(regime)
                        if draw<4 and suite in ('axis_1','axis_6'):
                            resolutions.setdefault(16*L,[]).append('double_density')
                        for r,regimes in resolutions.items():
                            categories=['camera']
                            if draw==0 and r==8*L and suite in ('axis_1','axis_6'):
                                categories+=['restricted','contradictory']
                            for category in categories:
                                domains=base_domains.copy()
                                if category=='restricted':
                                    cells=sorted(range(n),key=lambda v:(H('restriction_cell',gid,v),v))[:n//2]
                                    for v in cells:
                                        subsets=[m for m in range(1,8) if m&domains[v]==m and m&(1<<truth[v])]
                                        domains[v]=subsets[H('restriction_domain',gid,k,recipe,density,v)%len(subsets)]
                                # Robustness slots belong only to the primary regime, including when R coincides.
                                membership=regimes if category=='camera' else ['nominal_density']
                                slot=[category,gid,k,recipe,density,draw,r,suite]
                                for palette in PALETTES:
                                    c=dict(slot_id=slot,case_id=[slot,palette],category=category,geometry=gid,n=n,
                                           shape=g['shape_xyz'],k=k,selected_cells=selected,recipe=recipe,density=density,
                                           density_target=[a,b],occupancy=q,draw=draw,resolution=r,suite=suite,
                                           camera_indices=SUITES[suite],regimes=membership,palette=palette,
                                           truth=truth,truth_id=digest([gid,truth]),domains=domains,
                                           job_id=[category,gid,r,suite],expected_status='UNSAT' if category=='contradictory' else 'SAT')
                                    c['record_id']=digest(c['case_id'])
                                    sampled=category!='camera' or (draw==0 and 'nominal_density' in membership and suite in ('axis_1','axis_6'))
                                    c['queries']=queries(gid,n) if sampled else []
                                    c['reference_policy']=audit_camera(c)
                                    yield c

def graph_recipe(n,recipe):
    path=lambda start,stop:[(i,i+1) for i in range(start,stop-1)]
    edges=[]; fixed=[]; anchor=None
    if recipe in ('path','anchored_path'):
        edges=path(0,n); anchor=0 if recipe=='anchored_path' else None
    elif recipe=='even_cycle': edges=path(0,n)+[(0,n-1)]
    elif recipe=='odd_cycle_leaf': edges=path(0,n-1)+[(0,n-2),(n-2,n-1)]
    elif recipe=='repeated_edges': edges=[(v,v+1) for v in range(0,n,2)]
    elif recipe=='two_paths': edges=path(0,n//2)+path(n//2,n)
    elif recipe=='path_triangle': edges=path(0,n-3)+[(n-3,n-2),(n-3,n-1),(n-2,n-1)]
    elif recipe=='fixed_hit': fixed=list(range(n))
    elif recipe=='fixed_plus_path': edges=path(0,n//2); fixed=list(range(n//2,n))
    else: raise ValueError(recipe)
    return sorted(edges),fixed,anchor

def structured_designs():
    for n in PROTOCOL['structured']['cell_counts']:
        for spec in PROTOCOL['structured']['recipes']:
            recipe=spec['id']; edges,fixed,anchor=graph_recipe(n,recipe)
            for permutation in PROTOCOL['structured']['permutations']:
                if permutation=='identity': p=list(range(n))
                elif permutation=='reverse': p=list(reversed(range(n)))
                else: p=sorted(range(n),key=lambda v:(H('structured_vertex',n,int(permutation[-1]),v),v))
                for order in PROTOCOL['structured']['factor_orders']:
                    for palette in PALETTES:
                        fs=[]
                        for a,b in edges:
                            fs += [{'steps':[[p[a],1,0],[p[b],1,0]],'target':2},
                                   {'steps':[[p[a],0,1],[p[b],0,1]],'target':1 if palette=='split_material' else 2}]
                        if fixed: fs.append({'steps':[[p[v],1,1] for v in fixed],'target':2})
                        if anchor is not None: fs.append({'steps':[[p[anchor],1,0]],'target':2})
                        if order=='hashed': fs=[fs[i] for i in sorted(range(len(fs)),key=lambda i:(H('structured_factor',n,recipe,permutation,i),i))]
                        domains=[7]*n
                        for v in fixed: domains[p[v]]=3
                        slot=['structured',n,recipe,permutation,order]
                        yield dict(record_id=digest([slot,palette]),case_id=[slot,palette],slot_id=slot,
                                   category='structured',n=n,k=sum(m&6==6 for m in domains),recipe=recipe,
                                   permutation=permutation,factor_order=order,palette=palette,domains=domains,
                                   factors=fs,edges=[[p[a],p[b]] for a,b in edges],fixed_vertices=[p[v] for v in fixed],
                                   anchor_vertex=None if anchor is None else p[anchor],job_id=['structured',n,recipe],
                                   expected_status='UNSAT' if recipe in ('path_triangle','odd_cycle_leaf') else 'SAT',
                                   queries=queries([n,recipe,permutation],n),reference_policy='structured_exact',
                                   exhaustive_crosscheck=n<=8 or (permutation=='identity' and order=='canonical'))

def anchors():
    for name in ('path','triangle'):
        for palette in PALETTES:
            raw=json.loads(subprocess.check_output([str(BINARY),'anchor',str(int(name=='triangle')),str(PALETTES.index(palette))]))
            slot=['anchor',name]
            yield dict(**raw,record_id=digest([slot,palette]),case_id=[slot,palette],slot_id=slot,
                       category='anchor',n=8,k=3,palette=palette,fixture=name,job_id=['anchor',8],
                       queries=queries(name,8),reference_policy='exhaustive',expected_status='UNSAT' if name=='triangle' else 'SAT')

class RigBuilder:
    def __init__(self,g,resolution,out):
        self.g=g; self.resolution=resolution; self.out=Path(out); self.header=None; self.mapping=[]; self.cache={}
        self.path=self.out/'rigs'/f'{g["id"]}-r{resolution}.jsonl.gz'
        self.proc=subprocess.Popen([str(BINARY),'rig',*map(str,g['shape_xyz']),str(resolution)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        def rows():
            while True:
                line=self.proc.stdout.readline()
                if not line: raise RuntimeError('Rig preparation failed: '+self.proc.stderr.read())
                row=json.loads(line)
                if row['kind']=='rig_end': break
                if row['kind']=='rig': self.header=row
                else: self.mapping.append(row['factor'])
                yield row
        stream=rows(); context={**protocol_identity(),'kind':'raw_rational_rig','geometry':g['id'],'resolution':resolution}
        if self.path.exists():
            self.descriptor=verify_archive(self.path,expected_context=context)
            # Rendering still needs the independently built C++ rig in this process.
            for _ in stream: pass
        else:
            self.descriptor=write_archive(self.path,stream,context,1+8*resolution*resolution)
        self.descriptor['path']=self.path.relative_to(self.out).as_posix()
        if self.header is None: raise RuntimeError('Missing rig header')
        self.suite_factors={s:sorted({self.mapping[c*resolution*resolution+i] for c in cams for i in range(resolution*resolution)}) for s,cams in SUITES.items()}

    def materialize(self,c):
        w=tuple(c['truth'])
        if w not in self.cache:
            token=len(self.cache)
            self.proc.stdin.write(' '.join(map(str,[token,*w]))+'\n'); self.proc.stdin.flush()
            line=self.proc.stdout.readline()
            if not line: raise RuntimeError('Source truth render disagreement/error: '+self.proc.stderr.read())
            labels=json.loads(line)
            if labels['token']!=token: raise RuntimeError('Render response token mismatch')
            self.cache[w]=labels
        labels=self.cache[w][c['palette']]; indices=self.suite_factors[c['suite']]
        row={**c,'rig':self.descriptor['path'],'rig_sha256':self.descriptor['sha256'],
             'factor_indices':indices,'labels':[labels[i] for i in indices]}
        if c['category']=='contradictory':
            index=next((i for i in indices if self.header['factors'][i]),None)
            if index is None: raise RuntimeError('Protocol contradiction: no nonempty ray')
            target=2 if labels[index]==0 else 0
            row['factor_indices']=indices+[index]; row['labels']=row['labels']+[target]
            row['duplicate_ray_index']=next(i for i,f in enumerate(self.mapping) if f==index and i//(self.resolution**2) in c['camera_indices'])
        return row

    def close(self):
        self.proc.stdin.close(); code=self.proc.wait(timeout=30)
        if code: raise RuntimeError(self.proc.stderr.read())
        self.proc.stdout.close(); self.proc.stderr.close()

def prepare_subset(designs,out):
    """Preflight inputs are separate artifacts and never main timing records."""
    out=Path(out); groups={}; result=[]; rigs=[]
    for c in designs: groups.setdefault((c['geometry'],c['resolution']),[]).append(c)
    for (gid,r),cases in groups.items():
        rig=RigBuilder(GEOMETRIES[gid],r,out)
        try: result.extend(rig.materialize(c) for c in cases); rigs.append(rig.descriptor)
        finally: rig.close()
    return result,rigs

def generate_manifest(out):
    out=Path(out); out.mkdir(parents=True,exist_ok=True); start=time.monotonic(); context=protocol_identity()
    files=[]; rigs=[]; counts={}; query_slots=0; job_inventory={}; total=0
    def account(c):
        nonlocal total,query_slots
        total+=1; query_slots+=bool(c['queries']); counts[c['category']]=counts.get(c['category'],0)+1
        key=canonical(c['job_id']); j=job_inventory.setdefault(key,dict(job_id=c['job_id'],n=c['n'],case_slots=0,calls_per_epoch=0))
        j['case_slots']+=1; j['calls_per_epoch']+=len(MODES)*(2+len(c['queries']))
        return c
    for g in PROTOCOL['geometries']:
        designs=list(camera_designs(g)); by_r={}
        for c in designs: by_r.setdefault(c['resolution'],[]).append(c)
        for resolution,cases in sorted(by_r.items()):
            rig=RigBuilder(g,resolution,out)
            path=out/'cases'/f'{g["id"]}-r{resolution}.jsonl.gz'
            try:
                desc=write_archive(path,(account(rig.materialize(c)) for c in cases),
                                   {**context,'kind':'input_cases','geometry':g['id'],'resolution':resolution},len(cases))
            finally: rig.close()
            desc['path']=path.relative_to(out).as_posix(); files.append(desc); rigs.append(rig.descriptor)
            print(canonical({'prepared':desc['path'],'slots':len(cases)}),flush=True)
    for name,rows in [('structured',list(structured_designs())),('anchors',list(anchors()))]:
        path=out/'cases'/f'{name}.jsonl.gz'
        desc=write_archive(path,(account(c) for c in rows),{**context,'kind':'input_cases','category':name},len(rows))
        desc['path']=path.relative_to(out).as_posix(); files.append(desc)
    # Rebuild inventory from closed records, including cache hits, and check global uniqueness.
    total=query_slots=0; counts={}; job_inventory={}; ids=set()
    for entry in files:
        for c in read_records(out/entry['path']):
            if c['record_id'] in ids: raise ValueError('Duplicate design ID')
            ids.add(c['record_id']); account(c)
    planned=PROTOCOL['planned_counts']
    if total!=planned['total_slots'] or sum(j['calls_per_epoch'] for j in job_inventory.values())*5!=planned['total_timed_api_calls']:
        raise ValueError('Generated population differs from frozen protocol')
    if query_slots*6*4*5!=planned['sampled_query_timed_calls']:
        raise ValueError('Query population differs')
    manifest=dict(schema='MCR-A2-LARGER-INPUTS-1',complete=True,collection_started=False,
                  **context,source_files=source_identity(),design_slots=total,category_counts=counts,query_slots=query_slots,
                  timed_calls=planned['total_timed_api_calls'],files=files,rigs=rigs,
                  jobs=[job_inventory[k] for k in sorted(job_inventory)],
                  source_truth_validation='All original raw rays in both palettes: existing C++ primitive-AABB and traversal renderers agree, including the exact palette relabel.')
    atomic_json(out/'input-manifest.json',manifest)
    atomic_json(out/'preparation-receipt.json',{'seconds':time.monotonic()-start,'manifest_sha256':file_hash(out/'input-manifest.json')})
    print(canonical({'manifest':str(out/'input-manifest.json'),'design_slots':total,'timed_calls':manifest['timed_calls']}),flush=True)
    return manifest

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args(); generate_manifest(args.output)
