#!/usr/bin/env python3
"""Physical ray fixtures exposing a genuine local/global consistency gap."""
import json
from fractions import Fraction as Q
from pathlib import Path
import numpy as np
from phase_a import (Audit, Stats, make_factor, reference_ray, chain_render, gac,
                     oracle_support, shape_supports, solve, residual_components)


def main():
    # Other cells in the 2x2x2 bounding volume are explicitly fixed to air.
    cells = ((0,0,0),(1,0,0),(0,0,1))
    palette = (("air",0,False),("oak_bottom_slab",2,True),("stone_top_slab",1,2))
    worlds = np.array([[i%3,(i//3)%3,(i//9)%3] for i in range(27)],dtype=np.uint8)
    rays = []
    for pair in ("AB","AC","BC"):
        for height,target in ((Q(1,4),2),(Q(3,4),1)):
            if pair == "AB":
                c,d = (Q(-1),height,Q(1,2)),(Q(1),Q(0),Q(0))
            elif pair == "AC":
                c,d = (Q(1,2),height,Q(-1)),(Q(0),Q(0),Q(1))
            else:
                c,d = (Q(3),height,Q(-1)),(Q(-1),Q(0),Q(1))
            f = make_factor(c,d,(2,2,2),cells,palette)
            ref = reference_ray(c,d,cells,worlds,palette)
            assert np.array_equal(ref,chain_render(f,worlds))
            rays.append((f,ref,target))
    reports = []
    for name,count,expected_count in (("satisfiable_path",4,2),("infeasible_triangle",6,0)):
        factors = [x[0] for x in rays[:count]]
        target = [x[2] for x in rays[:count]]
        images = np.stack([x[1] for x in rays[:count]],axis=1)
        feasible = worlds[np.all(images == target,axis=1)]
        assert len(feasible) == expected_count
        audit = Audit(feasible)
        root = gac([7]*3,factors,target,Stats(),audit)
        assert root == [7,7,7], (name,root)
        stats = Stats()
        witness = solve([7]*3,factors,target,stats,audit)
        assert (witness is not None) == bool(len(feasible))
        support_stats = Stats()
        _,support,witnesses = shape_supports(3,3,factors,target,support_stats,audit)
        expected = oracle_support(feasible) if len(feasible) else None
        assert support == expected
        if witness is not None:
            assert any(np.array_equal(witness,w) for w in feasible)
        for w in witnesses:
            assert any(np.array_equal(w,x) for x in feasible)
        reports.append({
            "fixture":name,"reference_worlds":27,"feasible_worlds":len(feasible),
            "root_domains":root,"exact_supported_domains":support,
            "unsupported_survivors":sum((m & ~(expected[v] if expected else 0)).bit_count() for v,m in enumerate(root)),
            "component_sizes":[len(c) for c,_ in residual_components(root,factors,target)],
            "global_feasibility_search_nodes":stats.search_nodes,
            "global_feasibility_branches":stats.branches,
            "support_query_nodes":support_stats.search_nodes,
            "support_query_infeasible":support_stats.infeasible_queries,
            "max_unsat_support_query_nodes":support_stats.max_unsat_query_nodes,
            "audited_pruning_steps":audit.steps,
        })
    result = {"status":"PASS","description":"Exact one-pixel pinhole-ray observations of three cells; all other cells fixed air.","fixtures":reports}
    Path("adversarial_results.json").write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps(result,indent=2))


if __name__ == "__main__":
    main()
