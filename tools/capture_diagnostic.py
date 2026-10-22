#!/usr/bin/env python3
"""Controlled capture diagnostic. Truth is for comparison only; no solver is called."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import shutil
import sys

from capture_common import (COLORS, LABELS, STATES, boundary_mask, camera_read,
                            integer, json_read, keys, png_read, png_write, render_truth, require)

LEGEND = {
    'stable_exact_match': (35,180,80),
    'boundary_exact_match': (255,210,0),
    'foreground_background_disagreement': (240,40,40),
    'material_disagreement': (230,40,200),
    'unexpected_color': (40,110,255),
}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def crop_read(path, metadata):
    cap=json_read(path)
    keys(cap, 'schema minecraft_version source_width source_height crop world_to_model camera_setup')
    require(cap['schema']=='mcr-controlled-capture-v1' and cap['minecraft_version']=='Java Edition 1.21.1', 'unsupported capture profile/version')
    w=integer(cap['source_width'],1,4096); h=integer(cap['source_height'],1,4096)
    require(w*h <= 4194304, 'source image exceeds pixel bound')
    require(isinstance(cap['crop'],list) and len(cap['crop'])==4, 'crop must be [left,top,width,height]')
    x,y,cw,ch=cap['crop']
    integer(x,0,w-1); integer(y,0,h-1); integer(cw,1,64); integer(ch,1,64)
    require(x+cw<=w and y+ch<=h and (cw,ch)==(metadata['image']['width'],metadata['image']['height']), 'crop/camera dimension mismatch')
    require(cap['world_to_model']=={'x':'2-X','y':'Y-80','z':'Z'}, 'unsupported game-to-model transform')
    keys(cap['camera_setup'],'world_eye nominal_feet yaw_degrees pitch_degrees vertical_fov_degrees')
    require(cap['camera_setup']=={'world_eye':[1,81,14],'nominal_feet':[1,'3969/50',14], 'yaw_degrees':180,'pitch_degrees':0,'vertical_fov_degrees':90}, 'unsupported controlled camera setup')
    return cap


def truth_read(path):
    t=json_read(path); keys(t,'schema cell_order states purpose')
    require(t['schema']=='mcr-a2-example-truth-v1' and t['cell_order']=='id = (x * 2 + y) * 2 + z; y is vertical', 'unsupported truth schema/order')
    require(isinstance(t['states'],list) and len(t['states'])==8 and all(s in STATES for s in t['states']), 'invalid truth states')
    return [STATES.index(s) for s in t['states']]


def bbox(indices,w):
    return [min(i%w for i in indices),min(i//w for i in indices),max(i%w for i in indices),max(i//w for i in indices)] if indices else None


def diagnose(image,camera,capture,truth,output):
    m=camera_read(camera); cap=crop_read(capture,m); world=truth_read(truth)
    sw,sh,source=png_read(image)
    require((sw,sh)==(cap['source_width'],cap['source_height']), 'screenshot dimensions differ from declared framebuffer; do not resize')
    x,y,w,h=cap['crop']; pixels=[source[(y+r)*sw+x+c] for r in range(h) for c in range(w)]
    expected,ids=render_truth(m,world); boundary=boundary_mask(m,world)
    observed=[COLORS.index(p) if p in COLORS else None for p in pixels]
    counts=Counter(); categories=[]; known=[]; disagreements=[]; unknown=Counter()
    rows=['column\trow\tsource_column\tsource_row\tr\tg\tb\texpected\tobserved\texpected_cell\tboundary\tcategory']
    for i,(rgb,e,o,b) in enumerate(zip(pixels,expected,observed,boundary)):
        if o is None: category='unexpected_color'; unknown[rgb]+=1
        elif e==o: category='boundary_exact_match' if b else 'stable_exact_match'
        elif (e==0)!=(o==0): category='foreground_background_disagreement'
        else: category='material_disagreement'
        counts[category]+=1; categories.append(category)
        if o is not None and e!=o: disagreements.append(i)
        if category=='stable_exact_match': known.append([i%w,i//w,LABELS[o]])
        rows.append('\t'.join(map(str,[i%w,i//w,x+i%w,y+i//w,*rgb,LABELS[e],LABELS[o] if o is not None else 'unknown',ids[i],int(b),category])))
    exact=not unknown; match=exact and not disagreements
    report={
        'schema':'mcr-capture-diagnostic-v1','minecraft_version':cap['minecraft_version'],
        'claim':'Comparison against declared calibration and known truth only; not proof of Minecraft compatibility or calibration.',
        'solver_calls':0,'truth_used_for_solver':False,'pixels_recolored':0,'pixels_resampled':0,
        'input_sha256':{k:sha(v) for k,v in [('image',image),('camera',camera),('capture',capture),('truth',truth)]},
        'framebuffer':[sw,sh],'crop':[x,y,w,h],
        'strict_rgb_input_compatible':exact,'all_pixels_match_declared_truth':match,
        'status':'EXACT_MODEL_MATCH' if match else 'REQUIRES_INTERPRETATION',
        'counts':{k:counts[k] for k in LEGEND},
        'boundary':{'radius_pixels':1,'count':sum(boundary),'disagreement_count':sum(boundary[i] for i in disagreements),
                    'unexpected_color_count':sum(b and o is None for b,o in zip(boundary,observed)),
                    'method':'All projected truth-box edges including hidden edges; conservative warning band, not a complete rasterization error bound.'},
        'alignment':{'exact_label_disagreement_count':len(disagreements),
            'interior_exact_label_disagreement_count':sum(not boundary[i] for i in disagreements),
            'expected_foreground_bbox_inclusive':bbox([i for i,e in enumerate(expected) if e],w),
            'observed_exact_foreground_bbox_inclusive':bbox([i for i,o in enumerate(observed) if o in (1,2)],w),
            'interpretation':'Recognized-label spatial disagreement can indicate camera/crop/grid/scene mismatch; it does not identify a unique cause. Unknown colors cannot establish background or foreground.'},
        'appearance':{'unexpected_pixel_count':sum(unknown.values()),'distinct_unexpected_colors':len(unknown),
            'most_frequent_unexpected_colors':[{'rgb':list(k),'count':v} for k,v in sorted(unknown.items(),key=lambda kv:(-kv[1],kv[0]))[:32]],
            'interpretation':'Possible lighting, shading, filtering, sky, texture, gamma or postprocessing; no color fitting or substitution is performed.'},
        'known_observation_candidate_count':len(known),'per_pixel_evidence':'pixels.tsv',
        'known_observation_semantics':'Only exact-palette matches away from the warning band, conditional on the declared calibration and scene being correct. Not an inference input or an independent calibration proof.',
        'production_handoff':'raw-crop.png and camera.json contain no truth, labels or masks. The current solver requires EVERY pixel, including background and edges, to be exact. Do not discard unknown pixels or infer them from truth.',
        'overlay_legend':{k:list(v) for k,v in LEGEND.items()},
    }
    output=Path(output); require(not output.exists(), 'output directory already exists; preserve previous evidence')
    # Publish complete diagnostic directory only after writing all artifacts.
    staging=output.with_name(output.name+'.tmp'); staging.mkdir(parents=False,exist_ok=False)
    try:
        png_write(staging/'raw-crop.png',w,h,pixels)
        png_write(staging/'expected-from-truth.png',w,h,[COLORS[e] for e in expected])
        png_write(staging/'overlay.png',w,h,[LEGEND[c] for c in categories])
        png_write(staging/'boundary-mask.png',w,h,[(255,255,255) if b else (0,0,0) for b in boundary])
        comparison=[]
        for row in range(h):
            a=row*w; b=a+w
            comparison.extend(pixels[a:b]); comparison.extend(COLORS[e] for e in expected[a:b]); comparison.extend(LEGEND[c] for c in categories[a:b])
        png_write(staging/'comparison.png',w*3,h,comparison)
        (staging/'camera.json').write_bytes(Path(camera).read_bytes())
        (staging/'pixels.tsv').write_text('\n'.join(rows)+'\n')
        (staging/'report.json').write_text(json.dumps(report,indent=2,sort_keys=True)+'\n')
        (staging/'manifest.json').write_text(json.dumps({p.name:sha(p) for p in sorted(staging.iterdir())},indent=2,sort_keys=True)+'\n')
        staging.rename(output)
    except Exception:
        shutil.rmtree(staging); raise
    return report


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('image','camera','capture','truth','output-dir'): p.add_argument('--'+key,required=True,type=Path)
    a=p.parse_args()
    try:
        r=diagnose(a.image,a.camera,a.capture,a.truth,a.output_dir)
        print(r['status']); return 0 if r['all_pixels_match_declared_truth'] else 1
    except (ValueError,OSError,KeyError,TypeError,OverflowError,RecursionError) as exc:
        print('capture diagnostic: '+str(exc),file=sys.stderr); return 2


if __name__=='__main__': sys.exit(main())
