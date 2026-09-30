#!/usr/bin/env python3
"""Summaries of saved measurements only; never invokes an inference routine."""
import argparse
import json
from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt


def quantile(values, weights, probability):
    order = np.argsort(values, kind='stable')
    values, weights = values[order], weights[order]
    return float(values[min(len(values)-1, np.searchsorted(np.cumsum(weights), probability*weights.sum(), side='left'))])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('collection', type=Path)
    ap.add_argument('output', type=Path)
    args = ap.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    spec = json.loads((args.collection/'protocol.json').read_text())
    completion = json.loads((args.collection/'collection.json').read_text())
    assert completion['status'] == 'COMPLETE'
    summary, controls, queries, worst = [], [], [], []
    counts = dict(camera_families=0, perturbations=0, structured=0, camera_truth_memberships=0)
    metric_names = ['ns','nodes','branches','factor_updates','fixed_calls','decompositions','component_solves']
    for job in spec['jobs']:
        name = job['name']
        cases = pd.read_csv(args.collection/(name+'.cases.csv.gz')).set_index('case_id')
        runs = pd.read_csv(args.collection/(name+'.runs.csv.gz'), usecols=['case_id','mode','operation','status','repetition']+metric_names)
        assert set(runs.status) <= {'SAT','UNSAT'}, 'Report unresolved data separately before summarizing'
        camera = cases.kind.eq('camera')
        counts['camera_families'] += int(camera.sum())
        counts['perturbations'] += int(cases.kind.str.startswith('perturb_').sum())
        counts['structured'] += int(cases.kind.eq('structured').sum())
        counts['camera_truth_memberships'] += int(cases.loc[camera, 'family_size'].sum())
        query = runs[runs.operation.eq('query')]
        for mode, q in query.groupby('mode', sort=True):
            queries.append(dict(job=name, palette=job['palette'], k=job['k'], resolution=job['resolution'], mode=mode,
                                calls=len(q), mean_us=float(q.ns.mean()/1000), median_us=float(q.ns.median()/1000),
                                p95_us=float(q.ns.quantile(.95)/1000), max_nodes=int(q.nodes.max()),
                                max_branches=int(q.branches.max())))
        selected = runs[~runs.operation.eq('query')]
        aggregate = {key: ('median' if key=='ns' else 'first') for key in metric_names}
        measured = selected.groupby(['case_id','mode','operation'], sort=False).agg(aggregate).reset_index()
        measured = measured.join(cases, on='case_id')
        measured['microseconds'] = measured.ns/1000
        for (mode, op), group in measured.groupby(['mode','operation']):
            cameras = group[group.kind.eq('camera')]
            if not cameras.empty:
                row = cameras.loc[cameras.branches.idxmax()]
                worst.append(dict(job=name, k=job['k'], palette=job['palette'], resolution=job['resolution'], mode=mode,
                                  operation=op, case_id=int(row.case_id), suite=row.suite, truth_id=int(row.truth_id),
                                  nodes=int(row.nodes), branches=int(row.branches), largest_component=int(row.largest_component),
                                  residual_components=int(row.residual_components), shape_variables=int(row.shape_variables),
                                  family_size=int(row.family_size), supports=int(row.supports)))
            for suite, g in cameras.groupby('suite'):
                for weighting, col in [('truth','family_size'),('family',None),('occupancy_balanced','balanced_numerator')]:
                    weights = g[col].to_numpy(dtype=float) if col else np.ones(len(g))
                    def mean(values):
                        return float(np.average(np.asarray(values, dtype=float), weights=weights))
                    row = dict(job=name, k=job['k'], mask=job['mask'], arrangement=job['arrangement'],
                               palette=job['palette'], resolution=job['resolution'], suite=suite, mode=mode,
                               operation=op, weighting=weighting, families=len(g), truth_worlds=int(g.family_size.sum()),
                               mean_us=mean(g.microseconds), median_us=quantile(g.microseconds.to_numpy(),weights,.5),
                               p95_us=quantile(g.microseconds.to_numpy(),weights,.95), max_us=float(g.microseconds.max()),
                               mean_nodes=mean(g.nodes), max_nodes=int(g.nodes.max()),
                               mean_branches=mean(g.branches), max_branches=int(g.branches.max()),
                               search_fraction=mean(g.branches>0), mean_factor_updates=mean(g.factor_updates),
                               mean_fixed_calls=mean(g.fixed_calls), mean_decompositions=mean(g.decompositions),
                               mean_component_solves=mean(g.component_solves),
                               identified_fraction=mean(g.identified/8), unique_fraction=mean(g.family_size==1),
                               mean_family_size=mean(g.family_size), mean_free_cells=mean(g.free_cells),
                               mean_gac_gap=mean(g.gac_gap), mean_shape_variables=mean(g.shape_variables),
                               max_component=int(g.largest_component.max()), mean_component=mean(g.largest_component))
                    summary.append(row)
            if job['controls']:
                for _, row in group.iterrows():
                    controls.append(dict(palette=job['palette'], fixture=row.suite, mode=mode, operation=op,
                                         family_size=int(row.family_size), root_consistent=int(row.root_consistent),
                                         gac_gap=int(row.gac_gap), mean_us=float(row.microseconds),
                                         nodes=int(row.nodes), branches=int(row.branches), fixed_calls=int(row.fixed_calls),
                                         decompositions=int(row.decompositions), component_solves=int(row.component_solves)))
        print(name, 'summarized', flush=True)
    df = pd.DataFrame(summary)
    df.to_csv(args.output/'by-placement-suite.csv', index=False, float_format='%.9g')
    # Equal placement and view-suite weight at each k. Do not average percentiles.
    mean_cols = [c for c in df if c.startswith('mean_') or c.endswith('_fraction')]
    max_cols = [c for c in df if c.startswith('max_')]
    keys = ['resolution','palette','k','mode','operation','weighting']
    byk = df.groupby(keys).agg({**{c:'mean' for c in mean_cols}, **{c:'max' for c in max_cols}}).reset_index()
    byk.to_csv(args.output/'by-k.csv', index=False, float_format='%.9g')
    pd.DataFrame(controls).to_csv(args.output/'structured.csv', index=False, float_format='%.9g')
    pd.DataFrame(queries).to_csv(args.output/'sampled-queries.csv', index=False, float_format='%.9g')
    pd.DataFrame(worst).to_csv(args.output/'worst-cases.csv', index=False)
    (args.output/'analysis.json').write_text(json.dumps(dict(counts=counts,
        method='Per-family median of three timed calls; weighted within placement/suite, then equal placement and suite weights at k. Quantiles only in by-placement-suite.csv. All direct-query rows are saved; query summary is the deterministic sample, not a population estimate.',
        expected_occupied_cells_uniform={k: 4+k/6 for k in range(9)}, expected_occupied_cells_balanced=4,
        unresolved=completion['totals']['unresolved']), indent=2)+'\n')
    plt.rcParams.update({'font.size':10, 'axes.spines.top':False,'axes.spines.right':False,
                         'axes.grid':True,'grid.alpha':.18,'figure.facecolor':'white'})
    fig, axes = plt.subplots(2,2, figsize=(12,8.2), constrained_layout=True)
    colors = {'split':'#126f9c','same':'#c76824'}
    names = {'split':'Mixed materials','same':'Same material'}
    p = byk[(byk.resolution==8)&(byk.operation=='projection')]
    for palette in colors:
        for weight, style in [('truth','-'),('occupancy_balanced','--')]:
            g=p[(p.palette==palette)&(p['mode']=='both')&(p.weighting==weight)].sort_values('k')
            axes[0,0].plot(g.k,g.mean_branches,style,color=colors[palette],marker='o',markersize=3,
                           label=names[palette]+(' · uniform' if weight=='truth' else ' · balanced'))
        for mode, style in [('search','--'),('both','-')]:
            g=p[(p.palette==palette)&(p['mode']==mode)&(p.weighting=='truth')].sort_values('k')
            axes[0,1].plot(g.k,g.mean_branches,style,color=colors[palette],label=names[palette]+' · '+mode)
            axes[1,0].plot(g.k,g.mean_us,style,color=colors[palette],label=names[palette]+' · '+mode)
        for resolution, style in [(8,'-'),(16,'--')]:
            g=df[(df.palette==palette)&(df.resolution==resolution)&(df.suite=='axis_6')&
                 (df['mode']=='both')&(df.operation=='projection')&(df.weighting=='truth')]
            g=g.groupby('k').identified_fraction.mean().reset_index()
            axes[1,1].plot(g.k,100*g.identified_fraction,style,color=colors[palette],marker='o',markersize=3,
                          label=names[palette]+f' · {resolution}×{resolution}')
    axes[0,0].set(title='Density explains part of the branch growth',ylabel='Mean projection branches')
    axes[0,1].set(title='Search versus both optimizations',ylabel='Mean projection branches')
    axes[1,0].set(title='Measured latency includes budget hooks',ylabel='Mean of per-family medians (µs)')
    axes[1,1].set(title='Six-axis cell identifiability',ylabel='Identified cells (%)',ylim=(0,103))
    for ax in axes.flat:
        ax.set_xlabel('Cells allowing both slab halves, k'); ax.set_xticks(range(9)); ax.set_xlim(0,8)
        ax.legend(fontsize=8,frameon=False)
    fig.suptitle('A2 · replacement eight-cell sweep\nPaired palettes; exact image families; no larger-volume claim',fontsize=15)
    fig.savefig(args.output/'scaling.png',dpi=170)
    fig.savefig(args.output/'scaling.svg')
    plt.close(fig)


if __name__ == '__main__':
    main()
