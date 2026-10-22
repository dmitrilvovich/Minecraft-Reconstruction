#!/usr/bin/env python3
"""Bounded capture infrastructure checks. All images here are synthetic, not in-game."""
import argparse
import copy
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from capture_common import COLORS, camera_read, png_read, png_write, json_read
from capture_diagnostic import diagnose
from build_minecraft_capture_kit import build
from fixtures import raster, save
sys.path.insert(0,str(ROOT/'tests/png_demo'))
from oracle import enumerate_image, STATES, png_bytes


def run(binary, evidence):
    checks=[]; summaries={}; cli_calls=0; solver_calls=0
    def check(name,ok):
        if not ok: raise AssertionError(name)
        checks.append(name)
    with tempfile.TemporaryDirectory(prefix='mcr-capture-') as td:
        tmp=Path(td); kit=ROOT/'examples/minecraft-capture/kit'
        camera=kit/'camera.json'; capture=kit/'capture.json'; truth=kit/'truth.json'
        blocks=json_read(kit/'blocks.json'); pixels=raster(blocks); original=ROOT/'examples/minecraft-capture/fixtures/ideal-synthetic.png'
        check('committed fixture reproducible by independent world-space triangle rasterizer',png_read(original)==(640,480,pixels))
        regenerated=tmp/'kit';build(regenerated)
        check('all kit files including ZIPs reproduce byte-for-byte',{p.relative_to(kit).as_posix():p.read_bytes() for p in kit.rglob('*') if p.is_file()}=={p.relative_to(regenerated).as_posix():p.read_bytes() for p in regenerated.rglob('*') if p.is_file()})
        manifest=json_read(kit/'manifest.json')
        check('kit manifest hashes',all(hashlib.sha256((kit/p).read_bytes()).hexdigest()==v['sha256'] and (kit/p).stat().st_size==v['bytes'] for p,v in manifest['files'].items()))
        for name in ('mcr-flat-slabs-1.21.1.zip','mcr-capture-scene-1.21.1.zip'):
            with zipfile.ZipFile(kit/name) as z:
                check(name+' archive CRC and safe paths',z.testzip() is None and all(not n.startswith('/') and '..' not in Path(n).parts for n in z.namelist()))
        for block in blocks:
            x,y,z=block['model_position'];X,Y,Z=block['game_position']
            check('world-cell reflection '+str(block['id']),x==1-X and y==Y-80 and z==Z and block['id']==(x*2+y)*2+z)
        functions=kit/'data-pack/data/mcr/function'
        for palette in ('mixed','same'):
            lines=(functions/('setup_'+palette+'.mcfunction')).read_text().splitlines()
            actual=[]
            for line in lines:
                if line.startswith('setblock '):
                    match=re.fullmatch(r'setblock (-?\d+) (-?\d+) (-?\d+) minecraft:(oak|stone)_slab\[type=(bottom|top),waterlogged=false\]',line)
                    check(palette+' command contains only supported dry slab syntax '+str(len(actual)),match is not None)
                    actual.append((*map(int,match.groups()[:3]),match[4],match[5]))
            expected=[(*b['game_position'],'stone' if palette=='mixed' and b['state']=='top_slab' else 'oak','top' if b['state']=='top_slab' else 'bottom') for b in blocks if b['state']!='air']
            check(palette+' commands match truth positions/materials',actual==expected)
        check('correct 1.21 singular function paths and camera lock',json_read(kit/'data-pack/data/minecraft/tags/function/tick.json')=={'values':['mcr:tick']} and (functions/'tick.mcfunction').read_text()=='execute as @a[tag=mcr_capture] run teleport @s 1 79.38 14 180 0\n')
        resource=kit/'resource-pack/assets/minecraft/models/block'
        for mat in ('oak','stone'):
            for suffix,lo,hi in (('',0,8),('_top',8,16)):
                model=json_read(resource/(mat+'_slab'+suffix+'.json'))
                check(mat+suffix+' unchanged slab shape with shade/AO disabled',model['ambientocclusion'] is False and model['elements'][0]['from']==[0,lo,0] and model['elements'][0]['to']==[16,hi,16] and model['elements'][0]['shade'] is False)
        def diagnostic(name,image=original,cam=camera,cap=capture,tru=truth):
            out=tmp/name; r=diagnose(image,cam,cap,tru,out)
            summaries[name]={k:r[k] for k in ('status','counts','boundary','alignment','appearance','strict_rgb_input_compatible','all_pixels_match_declared_truth')}
            check(name+' calls no solver and changes no pixels',r['solver_calls']==r['pixels_recolored']==r['pixels_resampled']==0 and not r['truth_used_for_solver'])
            return r,out
        r,out=diagnostic('ideal')
        check('ideal full 4096-pixel agreement',r['all_pixels_match_declared_truth'] and sum(r['counts'].values())==4096)
        check('conservative edge warning retains exact input',0<r['boundary']['count']<4096 and r['known_observation_candidate_count']>0)
        crop=png_read(out/'raw-crop.png')[2]; check('unscaled crop exact byte samples',crop==[pixels[(208+y)*640+288+x] for y in range(64) for x in range(64)])
        check('camera handoff unchanged',(out/'camera.json').read_bytes()==camera.read_bytes())
        check('source-crop ray mapping analytically preserves pixel centers',all((x+288+0.5-320)/240==(x+0.5-32)/240 for x in range(64)))
        artifact_manifest=json.loads((out/'manifest.json').read_text()); check('diagnostic hashes',all(hashlib.sha256((out/p).read_bytes()).hexdigest()==v for p,v in artifact_manifest.items()))
        # Separate production handoff receives only raw pixels and camera, never truth.
        for palette in ('split_material','same_material'):
            cam=camera if palette=='split_material' else kit/'camera-same-material.json'
            src=tmp/(palette+'.png'); save(src,raster(blocks,palette)); rr,oo=diagnostic(palette,src,cam)
            dest=tmp/(palette+'-reconstruction.json')
            command=[str(binary),'--image',str(oo/'raw-crop.png'),'--camera',str(cam),'--output',str(dest)]
            result=subprocess.run(command,capture_output=True,text=True,timeout=60);solver_calls+=1
            check(palette+' actual C++ file reconstruction succeeds',result.returncode==0)
            solved=json.loads(dest.read_text()); worlds,supported,_=enumerate_image(camera_read(cam),png_read(oo/'raw-crop.png')[2])
            check(palette+' exact supports match all 6561 worlds',solved['feasibility']=='SAT' and [set(c['supported_states']) for c in solved['cells']]==[{STATES[s] for s in states} for states in supported])
            check(palette+' simultaneous witness independently valid',tuple(STATES.index(s) for s in solved['witness']['states']) in worlds)
            summaries[palette]['independent_feasible_worlds']=len(worlds)
        r,_=diagnostic('shaded',ROOT/'examples/minecraft-capture/fixtures/shaded-synthetic.png')
        check('shading detected without geometric accusations',r['appearance']['unexpected_pixel_count']>0 and r['alignment']['exact_label_disagreement_count']==0 and not r['strict_rgb_input_compatible'])
        r,_=diagnostic('shifted',ROOT/'examples/minecraft-capture/fixtures/shifted-synthetic.png')
        check('three-pixel shift creates interior alignment disagreements',r['appearance']['unexpected_pixel_count']==0 and r['alignment']['interior_exact_label_disagreement_count']>0)
        variants={}
        variants['mirror']=[pixels[y*640+639-x] for y in range(480) for x in range(640)]
        variants['sky']=[(100,160,220) if p==COLORS[0] else p for p in pixels]
        variants['material_swap']=[COLORS[1] if p==COLORS[2] else COLORS[2] if p==COLORS[1] else p for p in pixels]
        variants['missing_foreground']=[COLORS[0]]*(640*480)
        variants['blend']=[tuple((a+b)//2 for a,b in zip(p,COLORS[0])) if p!=COLORS[0] and pixels[max(0,i-1)]==COLORS[0] else p for i,p in enumerate(pixels)]
        for name,pp in variants.items():
            path=tmp/(name+'.png');save(path,pp);r,_=diagnostic(name,path)
            check(name+' detected',not r['all_pixels_match_declared_truth'])
            if name=='blend': check('blended edges identified',r['boundary']['unexpected_color_count']>0)
            if name=='material_swap':check('material information mismatch isolated',r['counts']['material_disagreement']>0 and r['counts']['foreground_background_disagreement']==0)
            if name=='sky':check('sky not silently treated as background',r['appearance']['unexpected_pixel_count']>0)
        for name,change in [('wrong_focal',('fx',220)),('wrong_principal',('cx',35)),('wrong_eye',('center',[1,'11/10',14]))]:
            m=json_read(camera);m['camera'][change[0]]=change[1]; path=tmp/(name+'.json');path.write_text(json.dumps(m))
            r,_=diagnostic(name,cam=path);check(name+' camera discrepancy detected',r['alignment']['exact_label_disagreement_count']>0 and r['appearance']['unexpected_pixel_count']==0)
        cap=json_read(capture);cap['crop'][0]+=3; cp=tmp/'wrong-crop.json';cp.write_text(json.dumps(cap));r,_=diagnostic('wrong_crop',cap=cp)
        check('crop offset mismatch detected',r['alignment']['exact_label_disagreement_count']>0)
        # RGB/RGBA encodings and every PNG scanline predictor preserve observations.
        encoded=tmp/'rgba.png';save(encoded,[(*p,255) for p in pixels],alpha=True,filters=(0,1,2,3,4));check('opaque RGBA and all PNG predictors decode',png_read(encoded)==(640,480,pixels))
        for name,data in [('truncated',original.read_bytes()[:-4]),('crc',original.read_bytes()[:-5]+b'\x00'+original.read_bytes()[-4:]),('transparent',png_bytes(1,1,[(1,2,3,0)],alpha=True))]:
            path=tmp/(name+'.png');path.write_bytes(data)
            try:png_read(path)
            except ValueError:check(name+' rejected',True)
            else:check(name+' rejected',False)
        def invalid(name,image=original,cam=camera,cap=capture,tru=truth):
            try:diagnose(image,cam,cap,tru,tmp/name)
            except (ValueError,TypeError):check(name+' rejected',True)
            else:check(name+' rejected',False)
        p=tmp/'small.png';png_write(p,1,1,[COLORS[0]]);invalid('wrong_dimensions',image=p)
        m=json_read(camera);m['camera']['right']=[1,0,0];p=tmp/'basis.json';p.write_text(json.dumps(m));invalid('wrong_handedness',cam=p)
        p=tmp/'duplicate.json';p.write_text('{"schema":1,"schema":2}');invalid('duplicate_metadata',cam=p)
        m=json_read(camera);m['camera']['fx']=240.0;p=tmp/'decimal.json';p.write_text(json.dumps(m));invalid('rounded_camera',cam=p)
        t=json_read(truth);t['states'][0]='stairs';p=tmp/'badtruth.json';p.write_text(json.dumps(t));invalid('unsupported_truth',tru=p)
        invalid('ideal')  # existing output must never be overwritten
        # Real CLI acceptance, rejection and deterministic complete artifacts.
        for name,image,expected_exit in [('cli1',original,0),('cli2',original,0),('cli_shift',ROOT/'examples/minecraft-capture/fixtures/shifted-synthetic.png',1),('cli_bad',tmp/'small.png',2)]:
            cmd=[sys.executable,str(ROOT/'tools/capture_diagnostic.py'),'--image',str(image),'--camera',str(camera),'--capture',str(capture),'--truth',str(truth),'--output-dir',str(tmp/name)]
            proc=subprocess.run(cmd,capture_output=True,text=True,timeout=30);cli_calls+=1;check(name+' exit status',proc.returncode==expected_exit)
        check('deterministic diagnostic files', {p.name:p.read_bytes() for p in (tmp/'cli1').iterdir()}=={p.name:p.read_bytes() for p in (tmp/'cli2').iterdir()})
    result={'schema':'mcr-capture-validation-v1','result':'PASS','actual_minecraft_screenshot_tested':False,'fixture_origin':'Independent world-space forward triangle rasterization; explicitly synthetic.', 'checks_passed':len(checks),'checks':checks,'diagnostic_cli_calls':cli_calls,'synthetic_reconstruction_calls':solver_calls,'exhaustive_reference_worlds_per_palette':6561,'cases':summaries}
    evidence.parent.mkdir(parents=True,exist_ok=True);evidence.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')
    print(f'PASS: {len(checks)} checks; {cli_calls} diagnostic CLI calls; {solver_calls} synthetic reconstruction calls; no Minecraft capture.')


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--binary',required=True,type=Path);p.add_argument('--evidence',required=True,type=Path)
    a=p.parse_args();run(a.binary.resolve(),a.evidence)
