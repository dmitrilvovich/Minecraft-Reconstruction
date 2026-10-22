#!/usr/bin/env python3
"""Reproduce the authored Java 1.21.1 capture kit; no game assets or solver execution."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile
from capture_common import COLORS, STATES, png_write

WORLD = [1,2,2,0,0,1,1,2]


def write_json(path,value):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(value,indent=2)+'\n')


def archive(root,path):
    with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for p in sorted(root.rglob('*')):
            if p.is_file():
                info=zipfile.ZipInfo(p.relative_to(root).as_posix(),date_time=(1980,1,1,0,0,0)); info.compress_type=zipfile.ZIP_DEFLATED; info.external_attr=0o100644<<16
                z.writestr(info,p.read_bytes(),compresslevel=9)


def build(out):
    out=Path(out); out.mkdir(parents=True,exist_ok=False)
    camera={'schema':'mcr-a2-camera-v1','grid':{'shape':[2,2,2],'origin':[0,0,0],'cell_size':1},'palette':'split_material','pixel_encoding':'mcr-a2-first-hit-rgb-v1','image':{'width':64,'height':64,'origin':'top_left'},'camera':{'center':[1,1,14],'forward':[0,0,-1],'right':[-1,0,0],'up':[0,1,0],'fx':240,'fy':240,'cx':32,'cy':32}}
    write_json(out/'camera.json',camera)
    camera['palette']='same_material'; write_json(out/'camera-same-material.json',camera)
    write_json(out/'truth.json',{'schema':'mcr-a2-example-truth-v1','cell_order':'id = (x * 2 + y) * 2 + z; y is vertical','states':[STATES[v] for v in WORLD],'purpose':'Known capture truth only; never an inference input.'})
    write_json(out/'capture.json',{'schema':'mcr-controlled-capture-v1','minecraft_version':'Java Edition 1.21.1','source_width':640,'source_height':480,'crop':[288,208,64,64],'world_to_model':{'x':'2-X','y':'Y-80','z':'Z'},'camera_setup':{'world_eye':[1,81,14],'nominal_feet':[1,'3969/50',14],'yaw_degrees':180,'pitch_degrees':0,'vertical_fov_degrees':90}})
    blocks=[]
    for cell,state in enumerate(WORLD):
        blocks.append({'id':cell,'model_position':[cell//4,(cell//2)%2,cell%2], 'game_position':[1-cell//4,80+(cell//2)%2,cell%2], 'state':STATES[state]})
    write_json(out/'blocks.json',blocks)
    resource=out/'resource-pack'; data=out/'data-pack'
    write_json(resource/'pack.mcmeta',{'pack':{'pack_format':34,'description':'MCR controlled flat slab appearance, Java 1.21.1; not fullbright'}})
    for material,color in [('oak',COLORS[2]),('stone',COLORS[1])]:
        tex=resource/f'assets/mcr/textures/block/{material}.png'; tex.parent.mkdir(parents=True,exist_ok=True); png_write(tex,16,16,[color]*256)
        for half in ('bottom','top'):
            # Authored geometry matches vanilla slab extents. No copied textures/shaders.
            faces={name:{'texture':'#all','uv':[0,0,16,16]} for name in ('down','up','north','south','west','east')}
            model={'ambientocclusion':False,'textures':{'all':f'mcr:block/{material}','particle':f'mcr:block/{material}'},'elements':[{'from':[0,0 if half=='bottom' else 8,0],'to':[16,8 if half=='bottom' else 16,16],'shade':False,'faces':faces}]}
            suffix='' if half=='bottom' else '_top'
            write_json(resource/f'assets/minecraft/models/block/{material}_slab{suffix}.json',model)
    write_json(data/'pack.mcmeta',{'pack':{'pack_format':48,'description':'MCR disposable controlled capture scene, Java 1.21.1'}})
    functions=data/'data/mcr/function'; functions.mkdir(parents=True)
    setup=['# ONLY run in a dedicated disposable capture world; clears the small capture region.',
        'gamemode spectator @s','effect clear @s','gamerule doDaylightCycle false','gamerule doWeatherCycle false','gamerule doMobSpawning false','time set noon','weather clear','fill -8 76 -4 9 85 18 minecraft:air']
    for palette in ('mixed','same'):
        commands=list(setup)
        for b in blocks:
            if b['state']=='air': continue
            mat='stone' if palette=='mixed' and b['state']=='top_slab' else 'oak'
            half='top' if b['state']=='top_slab' else 'bottom'
            commands.append('setblock '+' '.join(map(str,b['game_position']))+f' minecraft:{mat}_slab[type={half},waterlogged=false]')
        commands+=['function mcr:camera_on']
        (functions/f'setup_{palette}.mcfunction').write_text('\n'.join(commands)+'\n')
    (functions/'camera_on.mcfunction').write_text('tag @s add mcr_capture\nteleport @s 1 79.38 14 180 0\n')
    (functions/'camera_off.mcfunction').write_text('tag @s remove mcr_capture\n')
    (functions/'tick.mcfunction').write_text('execute as @a[tag=mcr_capture] run teleport @s 1 79.38 14 180 0\n')
    write_json(data/'data/minecraft/tags/function/tick.json',{'values':['mcr:tick']})
    (out/'options-reference.txt').write_text('# Reference for a dedicated Java 1.21.1 profile; do not overwrite an existing options.txt.\n# Set FOV in the UI to 90; saved normalized fov = (90-70)/40 = 0.5.\nfov:0.5\nfovEffectScale:0.0\nbobView:false\nao:false\nentityShadows:false\nmipmapLevels:0\nrenderClouds:false\ngamma:1.0\n# Launcher resolution 640 x 480; verify the actual saved PNG, not GUI scale.\n# First person, no shaders/mods, F1 hide HUD; F2 screenshot.\n')
    archive(resource,out/'mcr-flat-slabs-1.21.1.zip'); archive(data,out/'mcr-capture-scene-1.21.1.zip')
    write_json(out/'manifest.json',{'schema':'mcr-capture-kit-v1','minecraft_version':'Java Edition 1.21.1','actual_game_validation':False,'files':{p.relative_to(out).as_posix():{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(out.rglob('*')) if p.is_file()}})


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__); p.add_argument('--output',required=True,type=Path)
    build(p.parse_args().output)
