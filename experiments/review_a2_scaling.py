#!/usr/bin/env python3
"""Review committed measurements only. No renderer, solver, or chart generation.

The immutable input tree is the tree published at DATASET_COMMIT. Read all input
bytes through git, including the lossless NPZ transport, never a temporary CSV.
Dependencies: NumPy and pandas. Only subprocess command: read-only `git show`.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tarfile

import numpy as np
import pandas as pd

ROOT = Path(__file__).resolve().parents[1]
DATASET_COMMIT = '69032371f396802ddc5880f4cc65d8805eb60585'
DATASET_TREE = 'cc80882392543fbb0874164d8bfe0968bcede161'
PREFIX = 'results/a2-scaling-8cell/'
MODES = ['search', 'decomposition', 'fixed_hit', 'both']
OPS = ['feasibility', 'projection']
WORK = ['nodes', 'branches', 'factor_updates', 'gac_calls', 'fixed_calls',
        'literal_queries', 'infeasible_queries', 'decompositions',
        'component_solves', 'fixed_checks', 'fixed_rejections', 'witnesses']


def committed(path):
    return subprocess.check_output(['git', 'show', DATASET_TREE + ':' + path], cwd=ROOT)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def weighted_quantile(values, weights, probability):
    order = np.argsort(values, kind='stable')
    values, weights = np.asarray(values)[order], np.asarray(weights)[order]
    i = np.searchsorted(np.cumsum(weights), probability * weights.sum(), side='left')
    return float(values[min(int(i), len(values)-1)])


def summarize(g, weights):
    w = np.asarray(weights, dtype=float)
    def mean(x): return float(np.average(np.asarray(x), weights=w))
    result = dict(mean_us=mean(g.us), median_us=weighted_quantile(g.us, w, .5),
                  p95_us=weighted_quantile(g.us, w, .95), max_us=float(g.us.max()),
                  mean_trial_us=mean(g.trial_mean_us), max_raw_us=float(g.raw_max_us.max()))
    for col in ['nodes', 'branches', 'factor_updates', 'fixed_calls',
                'decompositions', 'component_solves', 'fixed_checks', 'fixed_rejections',
                'literal_queries']:
        result['mean_' + col] = mean(g[col])
    for col in ['nodes', 'branches']:
        result['p95_' + col] = weighted_quantile(g[col], w, .95)
        result['max_' + col] = int(g[col].max())
    result.update(search_fraction=mean(g.branches > 0), fixed_fraction=mean(g.fixed_calls > 0),
                  decomposition_fraction=mean(g.decompositions > 0),
                  identified_fraction=mean(g.identified/8), unique_fraction=mean(g.family_size == 1),
                  mean_family_size=mean(g.family_size), mean_free_cells=mean(g.free_cells),
                  mean_gac_gap=mean(g.gac_gap), mean_shape_variables=mean(g.shape_variables),
                  max_component=int(g.largest_component.max()), mean_component=mean(g.largest_component),
                  mean_root_components=mean(g.residual_components),
                  mean_supported_literals=mean(g.supported_literals))
    return result


def weights(g, name):
    if name == 'truth': return g.family_size.to_numpy(dtype=float)
    if name == 'occupancy_balanced': return g.balanced_numerator.to_numpy(dtype=float)
    return np.ones(len(g))


def compare_table(old, new, keys, name):
    old = old.set_index(keys).sort_index()
    new = new.set_index(keys).sort_index()
    assert old.index.equals(new.index), name + ': keys differ'
    mismatches, max_error, checked = [], 0.0, 0
    for col in old:
        assert col in new, (name, col)
        if pd.api.types.is_numeric_dtype(old[col]):
            a, b = old[col].to_numpy(), new[col].to_numpy()
            bad = ~np.isclose(a, b, rtol=6e-9, atol=1e-8, equal_nan=True)
            max_error = max(max_error, float(np.nanmax(np.abs(a-b))))
            checked += len(a)
            if bad.any(): mismatches.append(dict(column=col, cells=int(bad.sum())))
        else:
            bad = old[col].to_numpy() != new[col].to_numpy()
            if bad.any(): mismatches.append(dict(column=col, cells=int(bad.sum())))
    return dict(table=name, rows=len(old), numeric_cells_checked=checked,
                tolerance='rtol=6e-9, atol=1e-8; original CSVs use 9 significant digits',
                max_absolute_rounding_difference=max_error, mismatches=mismatches)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'results/a2-scaling-review')
    args = parser.parse_args(); out = args.output; out.mkdir(parents=True, exist_ok=True)
    manifest_bytes = committed(PREFIX+'raw/manifest.json')
    manifest = json.loads(manifest_bytes); parts = []
    for part in manifest['parts']:
        data = committed(PREFIX+'raw/'+part['name'])
        assert len(data) == part['bytes'] and sha(data) == part['sha256']
        parts.append(data)
    archive = b''.join(parts); assert sha(archive) == manifest['tar_sha256']
    tar = tarfile.open(fileobj=io.BytesIO(archive)); del parts
    def read(name): return tar.extractfile(name).read()
    transport = json.loads(read('transport.json'))
    for name, check in transport['files'].items():
        data = read(name)
        assert len(data) == check['bytes'] and sha(data) == check['sha256'], name
    def frame(name):
        with np.load(io.BytesIO(read(name)), allow_pickle=False) as z:
            schema = json.loads(z['schema'].tobytes()); columns = {}
            for i, col in enumerate(schema['columns']):
                a = z['c'+str(i)]
                columns[col['name']] = (pd.Categorical.from_codes(a, col['dictionary'])
                                        if 'dictionary' in col else a)
            result = pd.DataFrame(columns)
            assert len(result) == schema['rows']
            return result
    spec = json.loads(read('protocol.json')); provenance = json.loads(read('provenance.json'))
    completion = json.loads(read('collection.json'))
    assert sha(read('protocol.json')) == provenance['protocol_sha256'] == completion['protocol_sha256']
    assert sha(read('provenance.json')) == completion['provenance_sha256']
    for name in ['protocol.json', 'provenance.json', 'collection.json']:
        assert read(name) == committed(PREFIX+name)
    for path, expected in provenance['source_sha256'].items(): assert sha(committed(path)) == expected
    old = {name: pd.read_csv(io.BytesIO(committed(PREFIX+name))) for name in
           ['by-placement-suite.csv','by-k.csv','sampled-queries.csv','structured.csv','worst-cases.csv']}
    stratum_rows, k_rows, arrangement_rows, visibility_rows = [], [], [], []
    query_old, structured_old, worst_old, extremes = [], [], [], []
    pools, query_pools, diagnostics, counts = defaultdict(list), defaultdict(list), [], Counter()
    controls_population, query_exclusions, configuration_pairs = [], [], []
    six_axis, paired, rss = [], {}, []
    pool_cols = ['us','trial_mean_us','raw_max_us','nodes','branches','fixed_calls','component_solves',
                 'fixed_checks','fixed_rejections']
    def pool(g, job, mode, op):
        for (kind, answer, case_sat), part in g.groupby(['category','answer','case_sat'], observed=True):
            key = (job['resolution'], job['palette'], str(kind), mode, op, str(answer), bool(case_sat))
            values = part[pool_cols].to_numpy(dtype=float)
            pools[key].append(values)
            if op == 'query': query_pools[(job['resolution'],job['palette'],job['k'],str(kind),mode,op,str(answer),bool(case_sat))].append(values)
        for metric in ['nodes', 'branches', 'us', 'raw_max_us']:
            r = g.loc[g[metric].idxmax()]
            extremes.append(dict(job=job['name'], resolution=job['resolution'], palette=job['palette'],
                                 k=job['k'], mask=job['mask'], mode=mode, operation=op, criterion=metric,
                                 **{x: r[x].item() if isinstance(r[x], np.generic) else r[x] for x in
                                    ['case_id','kind','suite','truth_id','answer','family_size','supported_literals',
                                     'largest_component','residual_components','shape_variables','nodes','branches','us','raw_max_us']}))
    for k in range(9):
        k_chunks = defaultdict(list)
        for job in [j for j in spec['jobs'] if j['k'] == k]:
            name = job['name']; meta = json.loads(read(name+'.json'))
            assert meta['job'] == job
            c = frame(name+'.cases.npz'); r = frame(name+'.runs.npz')
            assert np.array_equal(c.case_id, np.arange(len(c)))
            assert len(c) == meta['counts']['cases'] and len(r) == meta['counts']['runs']
            counts.update({x: meta['counts'][x] for x in completion['totals']})
            assert int((r.status == 'UNRESOLVED').sum()) == meta['counts']['unresolved']
            assert int(r.witnesses.sum()) == meta['counts']['verified_witnesses']
            assert int((r.operation == 'query').sum()) == meta['counts']['direct_queries']
            c['supported_literals'] = sum(np.array([0,1,1,2,1,2,2,3])[(c.supports.to_numpy() >> (3*v)) & 7] for v in range(8))
            c['category'] = np.where(c.kind == 'camera', 'camera', np.where(c.kind == 'structured','structured','perturbation'))
            c['case_sat'] = c.family_size > 0
            for kind, g in c[c.kind!='camera'].groupby('kind',observed=True):
                controls_population.append(dict(job=name,resolution=job['resolution'],palette=job['palette'],k=k,
                    kind=str(kind),cases=len(g),sat=int(g.case_sat.sum()),unsat=int((~g.case_sat).sum()),
                    root_rejected=int((g.root_consistent==0).sum()),
                    unsat_after_nonempty_gac=int(((~g.case_sat)&(g.root_consistent!=0)).sum())))
            for kind, n in c['category'].value_counts().items(): counts['case_'+kind] += int(n)
            camera = c[c.kind == 'camera']
            counts['camera_truth_memberships'] += int(camera.family_size.sum())
            counts['camera_'+str(job['resolution'])] += len(camera)
            for suite, g in camera.groupby('suite', observed=True):
                assert int(g.family_size.sum()) == 3**k * 2**(8-k)
                assert int(g.balanced_numerator.sum()) == 2**(8+k)
            if not job['controls']:
                pert = c[c.category == 'perturbation'].sort_values(['suite','kind'])[['truth_id','domains']].to_numpy()
                pairkey = (job['mask'], job['resolution'])
                if pairkey in paired: assert np.array_equal(paired.pop(pairkey), pert)
                else: paired[pairkey] = pert
            rss.append(dict(job=name, peak_process_rss_kib=meta['counts']['peak_process_rss_kib']))
            mode_metrics = {}
            query_keys = None
            for mode in MODES:
                for op in OPS:
                    selected = r[(r['mode'] == mode) & (r.operation == op)].sort_values(['case_id','repetition'])
                    assert len(selected) == 3*len(c)
                    assert np.array_equal(selected.case_id, np.repeat(np.arange(len(c)),3))
                    assert np.array_equal(selected.repetition, np.tile(np.arange(3),len(c)))
                    ns = selected.ns.to_numpy().reshape(-1,3)
                    m = c.copy(); m['us'] = np.median(ns,axis=1)/1000
                    m['trial_mean_us'] = ns.mean(axis=1)/1000; m['raw_max_us'] = ns.max(axis=1)/1000
                    statuses = selected.status.to_numpy().reshape(-1,3)
                    assert (statuses == np.where(c.case_sat,'SAT','UNSAT')[:,None]).all()
                    m['answer'] = statuses[:,0]
                    for metric in WORK:
                        values = selected[metric].to_numpy().reshape(-1,3)
                        assert (values == values[:,0,None]).all(), (name,mode,op,metric)
                        m[metric] = values[:,0]
                    pool(m,job,mode,op)
                    mode_metrics[(mode,op)]=m[['category','answer','us','trial_mean_us','nodes','branches']].reset_index(drop=True)
                    cm = m[m.kind == 'camera']
                    if len(cm):
                        row = cm.loc[cm.branches.idxmax()]
                        worst_old.append(dict(job=name,k=k,palette=job['palette'],resolution=job['resolution'],mode=mode,
                            operation=op,**{x:row[x] for x in ['case_id','suite','truth_id','nodes','branches','largest_component',
                                                            'residual_components','shape_variables','family_size','supports']}))
                    for suite, g in cm.groupby('suite', observed=True):
                        for weighting in ['truth','family','occupancy_balanced']:
                            row = dict(job=name,k=k,mask=job['mask'],arrangement=job['arrangement'],palette=job['palette'],
                                       resolution=job['resolution'],suite=str(suite),mode=mode,operation=op,weighting=weighting,
                                       families=len(g),truth_worlds=int(g.family_size.sum()),**summarize(g,weights(g,weighting)))
                            stratum_rows.append(row)
                            if mode == 'both': visibility_rows.append(row)
                        k_chunks[(job['resolution'],job['palette'],mode,op)].append(g)
                        if mode == 'both':
                            for band, mask in [('unique',g.family_size==1),('ambiguous',g.family_size>1)]:
                                h=g[mask]
                                if len(h): diagnostics.append(dict(job=name,mask=job['mask'],resolution=job['resolution'],palette=job['palette'],k=k,
                                    suite=str(suite),operation=op,ambiguity=band,families=len(h),truth_memberships=int(h.family_size.sum()),
                                    branch_sum=int(h.branches.sum()),branching_families=int((h.branches>0).sum()),
                                    max_root_component=int(h.largest_component.max()),max_branches=int(h.branches.max())))
                        if job['resolution']==16 and suite=='axis_6':
                            assert (g.family_size==1).all() and (g.branches==0).all()
                            six_axis.append(dict(job=name,mode=mode,operation=op,families=len(g),max_nodes=int(g.nodes.max())))
                    if job['controls']:
                        for _, row in m.iterrows():
                            structured_old.append(dict(palette=job['palette'],fixture=row.suite,mode=mode,operation=op,
                                family_size=int(row.family_size),root_consistent=int(row.root_consistent),gac_gap=int(row.gac_gap),
                                mean_us=float(row.us),nodes=int(row.nodes),branches=int(row.branches),fixed_calls=int(row.fixed_calls),
                                decompositions=int(row.decompositions),component_solves=int(row.component_solves)))
                q = r[(r['mode']==mode)&(r.operation=='query')].copy()
                assert len(q)==24*int(c.queries_sampled.sum()) and not q.duplicated(['case_id','cell','state']).any()
                keys = q[['case_id','cell','state']].to_numpy()
                if query_keys is None: query_keys = keys
                else: assert np.array_equal(query_keys, keys)
                assert np.array_equal(np.sort(q.case_id.unique()), c.loc[c.queries_sampled != 0,'case_id'].to_numpy())
                ids=q.case_id.to_numpy(); expected=(c.supports.to_numpy()[ids] >> (3*q.cell.to_numpy()+q.state.to_numpy())) & 1
                assert (q.status.to_numpy()==np.where(expected,'SAT','UNSAT')).all()
                excluded=((c.domains.to_numpy()[ids] >> (3*q.cell.to_numpy()+q.state.to_numpy())) & 1)==0
                query_exclusions.append(dict(job=name,resolution=job['resolution'],palette=job['palette'],k=k,mode=mode,
                                             calls=len(q),input_excluded=int(excluded.sum())))
                query_old.append(dict(job=name,palette=job['palette'],k=k,resolution=job['resolution'],mode=mode,calls=len(q),
                                      mean_us=float(q.ns.mean()/1000),median_us=float(q.ns.median()/1000),
                                      p95_us=float(q.ns.quantile(.95)/1000),max_nodes=int(q.nodes.max()),max_branches=int(q.branches.max())))
                qm = c.iloc[ids].reset_index(drop=True)
                qm['us']=q.ns.to_numpy()/1000; qm['raw_max_us']=qm.us; qm['trial_mean_us']=qm.us
                qm['answer']=q.status.to_numpy()
                for metric in WORK: qm[metric]=q[metric].to_numpy()
                pool(qm,job,mode,'query')
                mode_metrics[(mode,'query')]=qm[['category','answer','us','trial_mean_us','nodes','branches']].reset_index(drop=True)
                if job['resolution']==16 and not job['controls']:
                    keep=(qm.suite=='axis_6')&(qm.kind=='camera')
                    assert (qm.loc[keep,'branches']==0).all()
                    counts['six_axis_16_query_calls_zero_branch']+=int(keep.sum())
            for op in OPS+['query']:
                for first,second in [('search','decomposition'),('search','fixed_hit'),('search','both'),('fixed_hit','both')]:
                    a,b=mode_metrics[(first,op)],mode_metrics[(second,op)]
                    assert a[['category','answer']].equals(b[['category','answer']])
                    for (category,answer),indices in a.groupby(['category','answer'],observed=True).groups.items():
                        x,y=a.loc[indices],b.loc[indices]
                        row=dict(job=name,k=k,resolution=job['resolution'],palette=job['palette'],operation=op,
                                 from_mode=first,to_mode=second,category=category,answer=answer,measurements=len(x))
                        for metric in ['branches','nodes','us','trial_mean_us']:
                            delta=y[metric].to_numpy(dtype=np.float64)-x[metric].to_numpy(dtype=np.float64)
                            row[metric+'_less']=int((delta<0).sum());row[metric+'_equal']=int((delta==0).sum());row[metric+'_more']=int((delta>0).sum())
                            row[metric+'_from_sum']=float(x[metric].sum());row[metric+'_to_sum']=float(y[metric].sum())
                        configuration_pairs.append(row)
            print('REVIEW',name,len(c),'saved cases',flush=True)
        for (resolution,palette,mode,op), chunks in k_chunks.items():
            combined=pd.concat(chunks,ignore_index=True)
            for weighting in ['truth','family','occupancy_balanced']:
                w=np.concatenate([weights(g,weighting)/weights(g,weighting).sum()/len(chunks) for g in chunks])
                k_rows.append(dict(resolution=resolution,palette=palette,k=k,mode=mode,operation=op,weighting=weighting,
                                   families=len(combined),strata=len(chunks),**summarize(combined,w)))
        del k_chunks
    assert not paired
    assert {x:counts[x] for x in completion['totals']}==completion['totals']
    assert counts['camera_8']==374771 and counts['camera_16']==112960 and counts['case_perturbation']==3456 and counts['case_structured']==12
    strata=pd.DataFrame(stratum_rows); byk=pd.DataFrame(k_rows)
    byk_keys=['resolution','palette','k','mode','operation','weighting']
    # The previous k table has no percentiles; verify its original mean-of-strata definition.
    means=[x for x in old['by-placement-suite.csv'] if x.startswith('mean_') or x.endswith('_fraction')]
    maxima=[x for x in old['by-placement-suite.csv'] if x.startswith('max_')]
    oldk=strata.groupby(byk_keys).agg({**{x:'mean' for x in means},**{x:'max' for x in maxima}}).reset_index()
    comparisons=[compare_table(old['by-placement-suite.csv'],strata,
                    ['job','suite','mode','operation','weighting'],'by-placement-suite.csv'),
                 compare_table(old['by-k.csv'],oldk,byk_keys,'by-k.csv'),
                 compare_table(old['sampled-queries.csv'],pd.DataFrame(query_old),['job','mode'],'sampled-queries.csv'),
                 compare_table(old['structured.csv'],pd.DataFrame(structured_old),['palette','fixture','mode','operation'],'structured.csv'),
                 compare_table(old['worst-cases.csv'],pd.DataFrame(worst_old),['job','mode','operation'],'worst-cases.csv')]
    assert not any(x['mismatches'] for x in comparisons), comparisons
    # Independent check of the new normalized-mixture k means against the earlier
    # average-of-strata calculation (not an average of medians or percentiles).
    for col in means+maxima:
        a=oldk.set_index(byk_keys)[col].sort_index();b=byk.set_index(byk_keys)[col].sort_index()
        assert np.allclose(a,b,rtol=1e-12,atol=1e-10),col
    def save(name,df): df.to_csv(out/name,index=False,float_format='%.12g')
    save('by-k-primary.csv',byk[byk.weighting!='occupancy_balanced'])
    save('by-k-occupancy-sensitivity.csv',byk[byk.weighting=='occupancy_balanced'])
    vis=pd.DataFrame(visibility_rows)
    viskeys=['resolution','palette','k','suite','operation','weighting']
    save('visibility.csv',vis.groupby(viskeys).agg({**{x:'mean' for x in means},**{x:'max' for x in maxima},
                                                 'mean_supported_literals':'mean','mean_root_components':'mean',
                                                 'fixed_fraction':'mean','decomposition_fraction':'mean'}).reset_index())
    arrkeys=['resolution','palette','k','mask','operation','weighting']
    save('arrangements.csv',vis.groupby(arrkeys).agg({x:'mean' for x in ['mean_us','mean_nodes','mean_branches','search_fraction',
         'unique_fraction','mean_family_size','mean_supported_literals','mean_root_components','mean_component']}).reset_index())
    save('structured.csv',pd.DataFrame(structured_old).rename(columns={'mean_us':'median_us'}))
    save('worst-per-job.csv',pd.DataFrame(extremes))
    save('ambiguity.csv',pd.DataFrame(diagnostics))
    save('whole-process-memory.csv',pd.DataFrame(rss))
    save('control-populations.csv',pd.DataFrame(controls_population))
    save('query-input-exclusions.csv',pd.DataFrame(query_exclusions))
    save('configuration-pairs.csv',pd.DataFrame(configuration_pairs))
    # These are descriptive pooled rows, not the primary equal-stratum weights.
    # F/P: one median-of-three per case; query: its one saved call per literal.
    def pooled_row(key,chunks):
        resolution,palette,category,mode,op,answer,case_sat=key
        a=np.concatenate(chunks);n=len(a)
        row=dict(resolution=resolution,palette=palette,category=category,mode=mode,operation=op,
                 answer=answer,case_sat=case_sat,measurements=n)
        for i,col in enumerate(pool_cols):
            values=a[:,i];row['mean_'+col]=float(values.mean());row['sum_'+col]=float(values.sum());row['max_'+col]=float(values.max())
            if col in ['us','nodes','branches']: row['median_'+col]=float(np.median(values));row['p95_'+col]=float(np.quantile(values,.95))
        row['branching_fraction']=float((a[:,4]>0).mean())
        return row
    pooled=[]
    for key, chunks in list(pools.items()):
        pools[(key[0],'ALL',*key[2:])].extend(chunks)
    for (resolution,palette,category,mode,op,answer,case_sat),chunks in pools.items():
        pooled.append(pooled_row((resolution,palette,category,mode,op,answer,case_sat),chunks))
    save('pooled-operation-status.csv',pd.DataFrame(pooled))
    sampled=[]
    for (resolution,palette,k,category,mode,op,answer,case_sat),chunks in query_pools.items():
        sampled.append(dict(k=k,**pooled_row((resolution,palette,category,mode,op,answer,case_sat),chunks)))
    save('sampled-queries-by-k.csv',pd.DataFrame(sampled))
    integrity=dict(status='PASS',dataset_commit=DATASET_COMMIT,dataset_tree=DATASET_TREE,
                   manifest_sha256=sha(manifest_bytes),archive_sha256=manifest['tar_sha256'],
                   parts=len(manifest['parts']),archived_files=len(transport['files']),jobs=len(spec['jobs']),
                   counts=dict(counts),previous_summary_comparisons=comparisons,
                   primary_weightings=['truth','family'],secondary_weighting='occupancy_balanced',
                   balanced_formula='family sum of 2^(k-occupied_incomparable_cells); denominator 2^(8+k)',
                   expected_occupancy_primary={k:4+k/6 for k in range(9)},expected_occupancy_balanced=4,
                   balanced_mass_verified_for_every_camera_stratum=True,all_work_counters_identical_across_repeats=True,
                   solver_cases_run=0,benchmarks_run=0,correctness_gates_run=0,sanitizer_gates_run=0,
                   data_sources='Committed NPZ measurement columns only; no inference or rendering imports',
                   analysis_environment={'python':sys.version.split()[0],'numpy':np.__version__,'pandas':pd.__version__},
                   six_axis_16_verification=six_axis,
                   methods={'by_k':'Normalize truth/family weights within placement/suite; then equal strata. Percentiles use the resulting mixture, never averaged percentiles.',
                            'runtime':'F/P case time is median of 3 saved calls; mean_trial_us separately includes all 3 trials; max_raw_us is maximum individual call.',
                            'pooled':'Unweighted family-case or sampled-literal descriptors across saved strata; not primary equal-stratum weighting.',
                            'query':'A query UNSAT means unsupported literal; case_sat records whole-scene feasibility. Deterministic sample is not a population estimator.',
                            'ambiguity':'Descriptive counts/sums by job/suite and family-size band; only mode both. Not causal inference.'})
    integrity['script_sha256']=sha(Path(__file__).read_bytes())
    integrity['outputs']={p.name:dict(bytes=p.stat().st_size,sha256=sha(p.read_bytes())) for p in sorted(out.glob('*.csv'))}
    (out/'review-integrity.json').write_text(json.dumps(integrity,indent=2)+'\n')
    print('PASS: committed rows only; previous numerical summaries reproduced; zero solver calls',flush=True)


if __name__=='__main__': main()
