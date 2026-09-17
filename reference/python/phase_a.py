#!/usr/bin/env python3
"""Exact, known-camera Phase A reconstruction experiment. Requires NumPy.

No camera fitting, textures, learned models, or structural priors are used.
The exhaustive renderer uses rational ray/AABB intersections. Inference uses
an independently constructed grid-plane traversal and finite ray automata.
Run: python phase_a.py --output results --all-a0 --samples 128
"""
from __future__ import annotations

import argparse
from collections import deque
from dataclasses import dataclass
from fractions import Fraction as Q
import csv
import json
import platform
from pathlib import Path
import resource
import time

import numpy as np


def values(mask):
    while mask:
        low = mask & -mask
        yield low.bit_length() - 1
        mask ^= low


@dataclass(frozen=True)
class RayFactor:
    cells: tuple
    emissions: tuple


@dataclass
class Stats:
    propagation_calls: int = 0
    deletions: int = 0
    factor_updates: int = 0
    search_nodes: int = 0
    branches: int = 0
    decompositions: int = 0
    fixed_kernel_calls: int = 0
    literal_queries: int = 0
    infeasible_queries: int = 0
    max_query_nodes: int = 0
    max_unsat_query_nodes: int = 0
    envelope_calls: int = 0


class Audit:
    """A read-only oracle hook. It never participates in inference decisions."""
    def __init__(self, feasible):
        self.feasible = feasible
        self.steps = 0
        self.literal_deletions = 0

    def begin(self, domains):
        f = self.feasible
        if len(f):
            keep = np.ones(len(f), dtype=bool)
            for v, mask in enumerate(domains):
                keep &= ((1 << f[:, v]) & mask) != 0
            f = f[keep]
        if len(f):
            return tuple(int(x) for x in np.bitwise_or.reduce(1 << f, axis=0))
        return (0,) * len(domains)

    def remove(self, supported, v, old, new):
        assert new & old == new
        assert (old ^ new) & supported[v] == 0, ("UNSOUND PRUNING", v, old, new, supported)
        self.steps += 1
        self.literal_deletions += (old ^ new).bit_count()

    def contradiction(self, supported):
        assert not any(supported), ("FALSE CONTRADICTION", supported)


def camera_specs(shape):
    target = tuple(Q(n, 2) for n in shape)
    distance = 3 * max(shape)
    axes = [
        ("minus_x", (1,0,0), (0,0,-1), (0,1,0)),
        ("minus_y", (0,1,0), (1,0,0), (0,0,-1)),
        ("plus_z", (0,0,-1), (-1,0,0), (0,1,0)),
        ("plus_x", (-1,0,0), (0,0,1), (0,1,0)),
        ("plus_y", (0,-1,0), (1,0,0), (0,0,1)),
        ("minus_z", (0,0,1), (1,0,0), (0,1,0)),
    ]
    f = (Q(2,3), Q(-2,3), Q(-1,3))
    r = (Q(1,3), Q(2,3), Q(-2,3))
    u = (Q(2,3), Q(1,3), Q(2,3))
    axes += [("oblique", f, r, u),
             ("opposite_oblique", tuple(-x for x in f), tuple(-x for x in r), u)]
    out = []
    for name, f, r, u in axes:
        f, r, u = tuple(map(Q, f)), tuple(map(Q, r)), tuple(map(Q, u))
        assert sum(x*x for x in f) == sum(x*x for x in r) == sum(x*x for x in u) == 1
        assert sum(f[j]*r[j] for j in range(3)) == 0
        assert sum(f[j]*u[j] for j in range(3)) == 0
        assert sum(r[j]*u[j] for j in range(3)) == 0
        assert (r[1]*u[2]-r[2]*u[1],r[2]*u[0]-r[0]*u[2],r[0]*u[1]-r[1]*u[0]) == f
        c = tuple(target[j] - distance*f[j] for j in range(3))
        out.append((name, c, f, r, u))
    return out


def pixels(spec, resolution):
    _, c, f, r, up = spec
    for b in range(resolution):
        for a in range(resolution):
            u = Q(2*a + 1 - resolution, 3*resolution)
            v = Q(2*b + 1 - resolution, 3*resolution)
            d = tuple(f[j] + u*r[j] + v*up[j] for j in range(3))
            yield c, d


def cells_for(shape):
    return tuple((x,y,z) for x in range(shape[0])
                 for y in range(shape[1]) for z in range(shape[2]))


def oracle_intersection(c, d, lo, hi):
    """Independent exact interval intersection; tangencies have zero length."""
    enter, leave = Q(0), None
    for j in range(3):
        if d[j] == 0:
            if not lo[j] <= c[j] < hi[j]:
                return None
        else:
            t0, t1 = (lo[j]-c[j])/d[j], (hi[j]-c[j])/d[j]
            t0, t1 = min(t0,t1), max(t0,t1)
            enter = max(enter,t0)
            leave = t1 if leave is None else min(leave,t1)
    return enter if leave is not None and enter < leave else None


def reference_ray(c, d, cells, world_array, palette):
    """Find nearest occupied primitive, using exact depth ranks."""
    count, n = world_array.shape
    domain_size = len(palette)
    hits = []
    for v, pos in enumerate(cells):
        for s, (_, color, half) in enumerate(palette):
            if s == 0:
                continue
            lo = tuple(Q(pos[j]) + (Q(1,2) if half == 2 and j == 1 else 0) for j in range(3))
            hi = tuple(Q(pos[j]) + (Q(1,2) if half == 1 and j == 1 else 1) for j in range(3))
            t = oracle_intersection(c,d,lo,hi)
            if t is not None:
                hits.append((t,v,s))
    hits.sort()
    sentinel = len(hits) + 1
    rank = np.full((n,domain_size),sentinel,dtype=np.int32)
    for k, (_,v,s) in enumerate(hits):
        rank[v,s] = k
    best = np.full(count,sentinel,dtype=np.int32)
    image = np.zeros(count,dtype=np.uint8)
    colors = np.array([x[1] for x in palette],dtype=np.uint8)
    for v in range(n):
        depth = rank[v,world_array[:,v]]
        update = depth < best
        image[update] = colors[world_array[update,v]]
        best[update] = depth[update]
    return image


def make_factor(c,d,shape,cells,palette):
    """Independent traversal from ALL grid-plane events and interval midpoints."""
    events = {Q(0)}
    for j in range(3):
        if d[j]:
            for k in range(shape[j]+1):
                t = (k-c[j])/d[j]
                if t > 0:
                    events.add(t)
    times = sorted(events)
    index = {p:v for v,p in enumerate(cells)}
    chain, emissions = [], []
    q = tuple(x.numerator // x.denominator for x in c)
    last_rank = -1
    for a,b in zip(times,times[1:]):
        midpoint = (a+b)/2
        point = tuple(c[j]+midpoint*d[j] for j in range(3))
        pos = tuple(x.numerator//x.denominator for x in point)
        if pos not in index:
            continue
        v = index[pos]
        rank = sum(abs(pos[j]-q[j]) for j in range(3))
        assert rank > last_rank, ("RANK FAILURE", c,d,pos)
        last_rank = rank
        boundaries = [a,b]
        if d[1]:
            h = (Q(pos[1])+Q(1,2)-c[1])/d[1]
            if a < h < b:
                boundaries.insert(1,h)
        slab_hit = any(Q(pos[1]) <= c[1]+((p+t)/2)*d[1] < Q(pos[1])+Q(1,2)
                       for p,t in zip(boundaries,boundaries[1:]))
        top_hit = any(Q(pos[1])+Q(1,2) <= c[1]+((p+t)/2)*d[1] < Q(pos[1])+1
                      for p,t in zip(boundaries,boundaries[1:]))
        emit = tuple(0 if s == 0 or (half == 1 and not slab_hit) or (half == 2 and not top_hit) else color
                     for s,(_,color,half) in enumerate(palette))
        chain.append(v)
        emissions.append(emit)
    return RayFactor(tuple(chain),tuple(emissions))


def chain_render(factor, worlds):
    image = np.zeros(len(worlds),dtype=np.uint8)
    live = np.ones(len(worlds),dtype=bool)
    for v,em in zip(factor.cells,factor.emissions):
        e = np.array(em,dtype=np.uint8)[worlds[:,v]]
        hit = live & (e != 0)
        image[hit] = e[hit]
        live &= e == 0
    return image


def build_observations(shape,palette,resolution):
    cells = cells_for(shape)
    base, n = len(palette), len(cells)
    ids = np.arange(base**n,dtype=np.int64)
    worlds = ((ids[:,None] // (base**np.arange(n,dtype=np.int64))) % base).astype(np.uint8)
    factors, columns, lookup, camera_factors = [], [], {}, []
    raw_rays = 0
    start = time.perf_counter()
    for spec in camera_specs(shape):
        indices = []
        for c,d in pixels(spec,resolution):
            factor = make_factor(c,d,shape,cells,palette)
            reference = reference_ray(c,d,cells,worlds,palette)
            predicted = chain_render(factor,worlds)
            assert np.array_equal(reference,predicted), ("RENDER ENCODING MISMATCH",spec[0])
            if factor not in lookup:
                lookup[factor] = len(factors)
                factors.append(factor)
                columns.append(reference)
            else:
                assert np.array_equal(reference,columns[lookup[factor]])
            indices.append(lookup[factor])
            raw_rays += 1
        camera_factors.append(tuple(indices))
    images = np.stack(columns,axis=1)
    return cells,worlds,factors,images,camera_factors,raw_rays,time.perf_counter()-start


def fixed_geometry(domains,factors,obs,stats,audit=None):
    """Complete feasibility for air + fixed opaque geometry/material domains."""
    stats.propagation_calls += 1
    stats.fixed_kernel_calls += 1
    domains = list(domains)
    supported = audit.begin(domains) if audit else None
    active = [m & ~1 for m in domains]
    required = [not (m & 1) for m in domains]
    incident = [[] for _ in domains]
    for ri,f in enumerate(factors):
        for v in f.cells:
            incident[v].append(ri)
    queue = deque(range(len(factors)))
    queued = set(queue)
    front = [0]*len(factors)
    while queue:
        ri = queue.popleft()
        queued.discard(ri)
        stats.factor_updates += 1
        f, target = factors[ri],obs[ri]
        k = front[ri]
        while k < len(f.cells):
            v,em = f.cells[k],f.emissions[k]
            if active[v] and any(em[s] != 0 for s in values(active[v])):
                break
            k += 1
        front[ri] = k
        if k == len(f.cells):
            if target != 0:
                if audit: audit.contradiction(supported)
                return None
            continue
        v,em = f.cells[k],f.emissions[k]
        allowed = sum(1 << s for s in values(active[v]) if em[s] == target and target != 0)
        new_active = active[v] & allowed
        if new_active != active[v]:
            old = domains[v]
            domains[v] = (old & 1) | new_active
            if audit: audit.remove(supported,v,old,domains[v])
            stats.deletions += (active[v] ^ new_active).bit_count()
            active[v] = new_active
            if not new_active:
                if required[v]:
                    if audit: audit.contradiction(supported)
                    return None
                for other in incident[v]:
                    if other not in queued:
                        queue.append(other)
                        queued.add(other)
    return domains


def fixed_supports(n,domain_size,factors,obs,stats,audit=None):
    initial = [(1<<domain_size)-1]*n
    domains = fixed_geometry(initial,factors,obs,stats,audit)
    if domains is None:
        return None,None
    domains = gac(domains,factors,obs,stats,audit)
    assert domains is not None
    support = [m & ~1 for m in domains]
    for v in range(n):
        if not domains[v] & 1:
            continue
        probe = list(domains)
        probe[v] = 1
        stats.literal_queries += 1
        if fixed_geometry(probe,factors,obs,stats,audit) is not None:
            support[v] |= 1
        else:
            stats.infeasible_queries += 1
    return domains,support


def transition(q,e,target):
    if q == 1:
        return 1
    if e == 0:
        return 0
    return 1 if e == target and target != 0 else None


def factor_supports(f,domains,target):
    m = len(f.cells)
    forward = [set() for _ in range(m+1)]
    forward[0] = {0}
    for j,(v,em) in enumerate(zip(f.cells,f.emissions)):
        for q in forward[j]:
            for s in values(domains[v]):
                nxt = transition(q,em[s],target)
                if nxt is not None:
                    forward[j+1].add(nxt)
    terminal = 0 if target == 0 else 1
    if terminal not in forward[m]:
        return None
    backward = [set() for _ in range(m+1)]
    backward[m] = {terminal}
    supported = [0]*m
    for j in range(m-1,-1,-1):
        v,em = f.cells[j],f.emissions[j]
        for q in (0,1):
            for s in values(domains[v]):
                if transition(q,em[s],target) in backward[j+1]:
                    backward[j].add(q)
                    if q in forward[j]:
                        supported[j] |= 1<<s
    return supported


def gac(domains,factors,obs,stats,audit=None):
    stats.propagation_calls += 1
    domains = list(domains)
    supported = audit.begin(domains) if audit else None
    incident = [[] for _ in domains]
    for ri,f in enumerate(factors):
        for v in f.cells:
            incident[v].append(ri)
    queue = deque(range(len(factors)))
    queued = set(queue)
    while queue:
        ri = queue.popleft()
        queued.discard(ri)
        stats.factor_updates += 1
        local = factor_supports(factors[ri],domains,obs[ri])
        if local is None:
            if audit: audit.contradiction(supported)
            return None
        for v,mask in zip(factors[ri].cells,local):
            new = domains[v] & mask
            if new != domains[v]:
                if audit: audit.remove(supported,v,domains[v],new)
                stats.deletions += (domains[v] ^ new).bit_count()
                domains[v] = new
                assert new
                for other in incident[v]:
                    if other not in queued:
                        queue.append(other)
                        queued.add(other)
    return domains


def output_set(f,domains):
    result = set()
    live = True
    for v,em in zip(f.cells,f.emissions):
        if not live:
            break
        possible = {em[s] for s in values(domains[v])}
        result |= possible - {0}
        live = 0 in possible
    if live:
        result.add(0)
    return result


def residual_components(domains,factors,obs):
    """Conservative exact-factorization graph, NOT an irreducible semantic graph."""
    scopes, active = [], []
    for ri,f in enumerate(factors):
        if output_set(f,domains) == {obs[ri]}:
            continue
        scope = set()
        for v,em in zip(f.cells,f.emissions):
            possible = {em[s] for s in values(domains[v])}
            if len(possible) > 1:
                scope.add(v)
            if 0 not in possible:
                break
        assert scope, ("Nonconstant constraint without scope",ri)
        active.append(ri)
        scopes.append(scope)
    adjacency = {}
    for scope in scopes:
        for v in scope:
            adjacency.setdefault(v,set()).update(scope-{v})
    remaining = set(adjacency)
    components = []
    while remaining:
        stack = [min(remaining)]
        comp = set()
        while stack:
            v = stack.pop()
            if v in comp:
                continue
            comp.add(v)
            stack.extend(adjacency[v]-comp)
        remaining -= comp
        ris = [ri for ri,scope in zip(active,scopes) if scope & comp]
        components.append((comp,ris))
    components.sort(key=lambda x:(-len(x[0]),min(x[0])))
    return components


def fixed_applicable(domains,factors):
    return all(len({em[s] != 0 for s in values(domains[v] & ~1)}) <= 1
               for f in factors for v,em in zip(f.cells,f.emissions))


def solve(domains,factors,obs,stats,audit=None):
    stats.search_nodes += 1
    domains = gac(domains,factors,obs,stats,audit)
    if domains is None:
        return None
    if fixed_applicable(domains,factors):
        result = fixed_geometry(domains,factors,obs,stats,audit)
        if result is None:
            return None
        return tuple(next(values(m & ~1)) if m & ~1 else 0 for m in result)
    components = residual_components(domains,factors,obs)
    if not components:
        return tuple(next(values(m)) for m in domains)
    if len(components) > 1:
        stats.decompositions += 1
        witness = [next(values(m)) for m in domains]
        for comp,ris in components:
            local = solve(domains,[factors[i] for i in ris],[obs[i] for i in ris],stats,audit)
            if local is None:
                return None
            for v in comp:
                witness[v] = local[v]
        return tuple(witness)
    comp,ris = components[0]
    degree = {v:sum(v in factors[i].cells for i in ris) for v in comp}
    v = min(comp,key=lambda v:(domains[v].bit_count(),-degree[v],v))
    for s in values(domains[v]):
        stats.branches += 1
        child = list(domains)
        child[v] = 1<<s
        result = solve(child,factors,obs,stats,audit)
        if result is not None:
            return result
    return None


def shape_supports(n,domain_size,factors,obs,stats,audit=None):
    initial = [(1<<domain_size)-1]*n
    root = gac(initial,factors,obs,stats,audit)
    if root is None:
        return None,None,[]
    support = [0]*n
    witnesses = []
    for v in range(n):
        for s in values(root[v]):
            if support[v] & (1<<s):
                continue
            probe = list(root)
            probe[v] = 1<<s
            stats.literal_queries += 1
            before = stats.search_nodes
            witness = solve(probe,factors,obs,stats,audit)
            used = stats.search_nodes-before
            stats.max_query_nodes = max(stats.max_query_nodes,used)
            if witness is not None:
                witnesses.append(witness)
                for j,t in enumerate(witness):
                    support[j] |= 1<<t
            else:
                stats.infeasible_queries += 1
                stats.max_unsat_query_nodes = max(stats.max_unsat_query_nodes,used)
    if not any(support):
        return root,None,witnesses
    return root,support,witnesses


def envelope_feasible(domains,factors,obs,ranks,stats,audit=None):
    """Complete for nested per-cell geometries, INCLUDING different materials.

    The rank table orders geometry by containment; tied ranks have identical
    hit geometry. No physical union is ever output as a fabricated block.
    """
    stats.propagation_calls += 1
    stats.envelope_calls += 1
    domains = list(domains)
    supported = audit.begin(domains) if audit else None
    incident = [[] for _ in domains]
    for ri,f in enumerate(factors):
        for v in f.cells:
            incident[v].append(ri)
    queue = deque(range(len(factors)))
    queued = set(queue)
    while queue:
        ri = queue.popleft()
        queued.discard(ri)
        stats.factor_updates += 1
        f,target = factors[ri],obs[ri]
        front = None
        for v,em in zip(f.cells,f.emissions):
            largest = max(values(domains[v]),key=lambda s:(ranks[s],-s))
            if em[largest]:
                front = (v,em)
                break
        if front is None:
            if target:
                if audit: audit.contradiction(supported)
                return None
            continue
        v,em = front
        new = sum(1<<s for s in values(domains[v]) if em[s] == 0 or em[s] == target)
        if new != domains[v]:
            if audit: audit.remove(supported,v,domains[v],new)
            stats.deletions += (domains[v]^new).bit_count()
            domains[v] = new
            if not new:
                if audit: audit.contradiction(supported)
                return None
            for other in incident[v]:
                if other not in queued:
                    queue.append(other)
                    queued.add(other)
    return tuple(max(values(m),key=lambda s:(ranks[s],-s)) for m in domains)


def nested_supports(n,domain_size,factors,obs,stats,audit=None,ranks=None):
    if ranks is None:
        ranks = (0,1,2)
    root = gac([(1<<domain_size)-1]*n,factors,obs,stats,audit)
    if root is None:
        return None,None,[]
    support,witnesses = [0]*n,[]
    for v in range(n):
        for s in values(root[v]):
            if support[v] & (1<<s):
                continue
            probe = list(root)
            probe[v] = 1<<s
            stats.literal_queries += 1
            w = envelope_feasible(probe,factors,obs,ranks,stats,audit)
            if w is None:
                stats.infeasible_queries += 1
                continue
            witnesses.append(w)
            for j,t in enumerate(w):
                support[j] |= 1<<t
    return root,(support if any(support) else None),witnesses


def oracle_support(feasible):
    return [int(x) for x in np.bitwise_or.reduce(1<<feasible,axis=0)]


def freedom_count(ids,worlds,base):
    if len(ids) < base:
        return 0
    total = 0
    for v in range(worlds.shape[1]):
        context = ids - worlds[ids,v].astype(np.int64)*(base**v)
        _,counts = np.unique(context,return_counts=True)
        total += bool(np.all(counts == base))
    return total


def adversarial_ids(cells,base):
    scenes = []
    scenes.append([0]*len(cells))
    scenes.append([1]*len(cells))
    scenes.append([2]*len(cells))
    scenes.append([1 if x == 0 else 2 for x,y,z in cells])
    scenes.append([1 if x == 0 else 0 for x,y,z in cells])
    scenes.append([1 if x == 1 else 0 for x,y,z in cells])
    scenes.append([1+(x+y+z)%2 for x,y,z in cells])
    scenes.append([0 if y == z == 0 else 1 for x,y,z in cells])
    for v in range(len(cells)):
        for s in range(1,base):
            w = [0]*len(cells)
            w[v] = s
            scenes.append(w)
    if base == 4:
        scenes.extend([
            [3 if x == 0 else 1 for x,y,z in cells],
            [3 if y == 0 else 2 for x,y,z in cells],
            [3 if (x+z)%2 else 2 for x,y,z in cells],
            [1 if x == 1 else 3 for x,y,z in cells],
        ])
    return sorted({sum(s*base**v for v,s in enumerate(w)) for w in scenes})


def view_suites(camera_factors):
    out = []
    for count in range(1,7):
        indices = sorted({r for cam in camera_factors[:count] for r in cam})
        out.append((f"axis_{count}",indices,count))
    out.append(("oblique_1",sorted(set(camera_factors[6])),1))
    out.append(("oblique_2",sorted(set(camera_factors[6]+camera_factors[7])),2))
    return out


def grouped(images,indices):
    selected = images[:,indices]
    groups = {}
    for world_id,row in enumerate(selected):
        groups.setdefault(row.tobytes(),[]).append(world_id)
    return groups


def test_phase(name,palette,args):
    shape = (2,2,2)
    start = time.perf_counter()
    cells,worlds,all_factors,images,camera_factors,raw_rays,render_seconds = build_observations(shape,palette,args.resolution)
    print(json.dumps({"phase":name,"worlds":len(worlds),"raw_rays":raw_rays,
                      "distinct_factors":len(all_factors),"render_seconds":round(render_seconds,3)}),flush=True)
    n,base = len(cells),len(palette)
    summaries,rows = [],[]
    rng = np.random.default_rng(args.seed)
    audit_steps = 0
    catalog = adversarial_ids(cells,base)
    for suite,indices,views in view_suites(camera_factors):
        groups = grouped(images,indices)
        keys = list(groups)
        wanted = {images[w,indices].tobytes() for w in catalog}
        all_cases = ((name == "A0" and args.all_a0) or (name == "A1" and args.all_a1) or (name == "A2" and args.all_a2)) and suite in ("axis_1","axis_3","axis_6")
        if all_cases:
            wanted = set(keys)
        else:
            select = rng.choice(len(keys),size=min(args.samples,len(keys)),replace=False)
            wanted.update(keys[int(j)] for j in select)
        factor_list = [all_factors[i] for i in indices]
        identifiable = 0
        freely_variable = 0
        singleton_worlds = 0
        group_sizes = []
        marginals = {}
        strata = {k:{"worlds":0,"identified_cells":0,"unique_worlds":0} for k in range(n+1)} if name == "A1" else {}
        for key,members in groups.items():
            ids = np.array(members,dtype=np.int64)
            f = worlds[ids]
            support = oracle_support(f)
            fixed = sum(m.bit_count() == 1 for m in support)
            identifiable += len(ids)*fixed
            freely_variable += len(ids)*freedom_count(ids,worlds,base)
            singleton_worlds += len(ids) == 1
            group_sizes.append(len(ids))
            if strata:
                slab_counts = np.sum(f == 3,axis=1)
                for k,count in enumerate(np.bincount(slab_counts,minlength=n+1)):
                    strata[k]["worlds"] += int(count)
                    strata[k]["identified_cells"] += int(count)*fixed
                    if len(ids) == 1:
                        strata[k]["unique_worlds"] += int(count)
            if key in wanted:
                marginals[key] = (ids,support)
        suite_start = time.perf_counter()
        for key in keys:
            if key not in wanted:
                continue
            ids,expected = marginals[key]
            feasible = worlds[ids]
            obs = list(key)
            audit = Audit(feasible)
            audited_stats = Stats()
            if name == "A0":
                root,support = fixed_supports(n,base,factor_list,obs,audited_stats,audit)
                witnesses = []
            elif name == "A2" or (name == "A1" and args.a1_mode == "search"):
                root,support,witnesses = shape_supports(n,base,factor_list,obs,audited_stats,audit)
            else:
                ranks = (0,2,2,1) if name == "A1" else (0,1,2)
                root,support,witnesses = nested_supports(n,base,factor_list,obs,audited_stats,audit,ranks)
            assert support == expected, ("SUPPORT MISMATCH",name,suite,int(ids[0]),support,expected)
            if witnesses:
                accepted = set(map(int,ids))
                for w in witnesses:
                    witness_id = sum(s*base**v for v,s in enumerate(w))
                    assert witness_id in accepted, ("BAD WITNESS",w)
            audit_steps += audit.steps
            stats = Stats()
            t0 = time.perf_counter()
            if name == "A0":
                root2,support2 = fixed_supports(n,base,factor_list,obs,stats)
            elif name == "A2" or (name == "A1" and args.a1_mode == "search"):
                root2,support2,_ = shape_supports(n,base,factor_list,obs,stats)
            else:
                ranks = (0,2,2,1) if name == "A1" else (0,1,2)
                root2,support2,_ = nested_supports(n,base,factor_list,obs,stats,ranks=ranks)
            elapsed = time.perf_counter()-t0
            assert root2 == root and support2 == support
            components = residual_components(root,factor_list,obs)
            unsupported = sum((r & ~s).bit_count() for r,s in zip(root,support))
            if name == "A1":
                shape_vars = {v for v,m in enumerate(root) if m & 8 and m & 6}
            elif name in ("Amono","A2"):
                shape_vars = {v for v,m in enumerate(root) if m & 2 and m & 4}
            else:
                shape_vars = set()
            coupled_shapes = [len(comp & shape_vars) for comp,_ in components]
            rows.append({
                "phase":name,"suite":suite,"views":views,"representative_id":int(ids[0]),
                "feasible_worlds":len(ids),"initial_candidates":n*base,
                "root_candidates":sum(m.bit_count() for m in root),
                "root_domains":json.dumps(root),
                "root_removed":n*base-sum(m.bit_count() for m in root),
                "root_unsupported_survivors":unsupported,
                "exact_candidates":sum(m.bit_count() for m in support),
                "identifiable_cells":sum(m.bit_count()==1 for m in support),
                "freely_variable_cells":freedom_count(ids,worlds,base),
                "components":len(components),
                "component_sizes":json.dumps([len(c) for c,_ in components]),
                "shape_component_sizes":json.dumps(coupled_shapes),
                "largest_component":max([len(c) for c,_ in components],default=0),
                "shape_variables":len(shape_vars),"search_nodes":stats.search_nodes,
                "unresolved_variables":sum(m.bit_count()>1 for m in root),
                "occupancy_variables":sum(bool(m & 1 and m & ~1) for m in root),
                "literal_queries":stats.literal_queries,"infeasible_queries":stats.infeasible_queries,
                "max_query_nodes":stats.max_query_nodes,"max_unsat_query_nodes":stats.max_unsat_query_nodes,
                "branches":stats.branches,"propagation_calls":stats.propagation_calls,
                "fixed_kernel_calls":stats.fixed_kernel_calls,
                "envelope_calls":stats.envelope_calls,
                "factor_updates":stats.factor_updates,"seconds":elapsed,
                "audited_pruning_steps":audit.steps,
            })
        current = [row for row in rows if row["suite"] == suite]
        summary = {
            "suite":suite,"views":views,"distinct_observation_classes":len(groups),
            "tested_classes":len(current),"all_classes_tested":all_cases or len(current)==len(groups),
            "oracle_identifiable_fraction_uniform_world":identifiable/(len(worlds)*n),
            "oracle_freely_variable_fraction_uniform_world":freely_variable/(len(worlds)*n),
            "oracle_unique_world_fraction":singleton_worlds/len(worlds),
            "max_feasible_worlds":max(group_sizes),
            "tested_max_branches":max(r["branches"] for r in current),
            "tested_max_search_nodes":max(r["search_nodes"] for r in current),
            "tested_max_root_unsupported_survivors":max(r["root_unsupported_survivors"] for r in current),
            "tested_max_component":max(r["largest_component"] for r in current),
            "solver_seconds_median":float(np.median([r["seconds"] for r in current])),
            "solver_seconds_max":max(r["seconds"] for r in current),
            "suite_validation_seconds":time.perf_counter()-suite_start,
            "oracle_by_true_slab_count":{
                str(k):{"worlds":x["worlds"],
                        "identifiable_fraction":x["identified_cells"]/(x["worlds"]*n),
                        "unique_world_fraction":x["unique_worlds"]/x["worlds"]}
                for k,x in strata.items() if x["worlds"]},
        }
        summaries.append(summary)
        print(json.dumps({"phase":name,**summary}),flush=True)
    # Inconsistent observation: duplicate one nonempty ray with incompatible colors.
    f = next(f for f in all_factors if f.cells)
    factors,obs = [f,f],[1,2]
    assert not any(np.all(np.stack([chain_render(f,worlds),chain_render(f,worlds)],axis=1)==obs,axis=1))
    audit = Audit(worlds[:0])
    if name == "A0":
        root,support = fixed_supports(n,base,factors,obs,Stats(),audit)
        assert support is None
    elif name == "A2" or (name == "A1" and args.a1_mode == "search"):
        root,support,_ = shape_supports(n,base,factors,obs,Stats(),audit)
        assert support is None
    else:
        ranks = (0,2,2,1) if name == "A1" else (0,1,2)
        root,support,_ = nested_supports(n,base,factors,obs,Stats(),audit,ranks)
        assert support is None
    return {
        "phase":name,"shape":shape,"palette":palette,"worlds_enumerated":len(worlds),
        "raw_rays":raw_rays,"distinct_ray_factors":len(all_factors),
        "renderer_equalities_checked":len(worlds)*raw_rays,
        "audited_pruning_steps":audit_steps,"render_seconds":render_seconds,
        "total_seconds":time.perf_counter()-start,"suites":summaries,
        "infeasible_duplicate_ray_test":"passed",
    },rows


def logical_regressions():
    # Full marginal domain != an observation-independent cell.
    worlds = np.array([[code%3,(code//3)%3] for code in range(9)],dtype=np.uint8)
    f = RayFactor((0,1),((0,1,2),(0,1,2)))
    obs = chain_render(f,worlds)
    ids = np.flatnonzero(obs == 1)
    assert len(ids) == 4
    assert oracle_support(worlds[ids]) == [3,7]
    assert freedom_count(ids,worlds,3) == 0
    fixed_prefix = ids[worlds[ids,0] == 1]
    assert freedom_count(fixed_prefix,worlds,3) == 1
    # A deliberately bad pruning step must be rejected by the audit.
    audit = Audit(worlds[ids])
    caught = False
    try:
        audit.remove(audit.begin([7,7]),1,7,3)  # Incorrectly remove hidden oak.
    except AssertionError:
        caught = True
    assert caught
    # Exact tangency and boundary conventions; no arbitrary floating tolerance.
    assert oracle_intersection((Q(-1),Q(1),Q(0)),(Q(1),Q(0),Q(0)),(Q(0),Q(0),Q(0)),(Q(1),Q(1),Q(1))) is None
    assert oracle_intersection((Q(-1),Q(0),Q(0)),(Q(1),Q(0),Q(0)),(Q(0),Q(0),Q(0)),(Q(1),Q(1),Q(1))) == 1
    return {"correlation_vs_freedom":"passed","deliberately_unsound_prune_detected":"passed","boundary_conventions":"passed"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output",default="results")
    parser.add_argument("--resolution",type=int,default=8)
    parser.add_argument("--samples",type=int,default=128)
    parser.add_argument("--seed",type=int,default=20260917)
    parser.add_argument("--all-a0",action="store_true")
    parser.add_argument("--all-a1",action="store_true")
    parser.add_argument("--all-a2",action="store_true")
    parser.add_argument("--a1-mode",choices=("envelope","search"),default="envelope")
    args = parser.parse_args()
    out = Path(args.output)
    out.mkdir(parents=True,exist_ok=True)
    palettes = [
        ("A0",(("air",0,False),("stone_cube",1,False),("oak_cube",2,False))),
        ("A1",(("air",0,False),("stone_cube",1,False),("oak_cube",2,False),("oak_bottom_slab",2,True))),
        ("Amono",(("air",0,False),("oak_bottom_slab",1,True),("oak_cube",1,False))),
        ("A2",(("air",0,False),("oak_bottom_slab",2,True),("stone_top_slab",1,2))),
    ]
    summaries,rows = [],[]
    regressions = logical_regressions()
    for name,palette in palettes:
        summary,phase_rows = test_phase(name,palette,args)
        summaries.append(summary)
        rows.extend(phase_rows)
        (out/f"{name}_summary.json").write_text(json.dumps(summary,indent=2)+"\n")
    metadata = {
        "status":"PASS","arguments":vars(args),"python":platform.python_version(),
        "logical_regressions":regressions,
        "numpy":np.__version__,"platform":platform.platform(),
        "peak_process_rss_kib":resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "measurement_note":"Solver timings exclude exhaustive reference and pruning audit; peak RSS includes the entire process and oracle.",
        "phases":summaries,
    }
    (out/"results.json").write_text(json.dumps(metadata,indent=2)+"\n")
    with (out/"cases.csv").open("w",newline="") as file:
        writer = csv.DictWriter(file,fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    print(json.dumps({"status":"PASS","tested_cases":len(rows),"peak_process_rss_kib":metadata["peak_process_rss_kib"]}),flush=True)


if __name__ == "__main__":
    main()
