"""Complete file-based PNG -> executable -> JSON verification; no research runs."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib

from oracle import COLORS, STATES, chunk, enumerate_image, png_bytes, read_png, render, sample


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--examples', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args(); binary = args.binary.resolve()
    checks = []; calls = 0; enumerations = []

    def check(name, condition):
        if not condition: raise AssertionError(name)
        checks.append({'check': name, 'result': 'PASS'})

    def invoke(image, camera, output, *, extra=()):
        nonlocal calls
        calls += 1
        return subprocess.run([str(binary),'--image',str(image),'--camera',str(camera),'--output',str(output),*extra],
                              capture_output=True,text=True,timeout=30)

    def verify(name, metadata, pixels, result):
        worlds,supported,constraints = enumerate_image(metadata,pixels)
        enumerations.append({'case':name,'worlds_enumerated':3**8,'feasible_worlds':len(worlds),
                             'input_pixels':len(pixels),'supported_state_codes':[sorted(s) for s in supported]})
        check(name+': complete SAT/UNSAT matches exhaustive oracle',
              result['feasibility'] == ('SAT' if worlds else 'UNSAT') and result['projection_complete'])
        check(name+': exact marginals equal exhaustive world union',
              [[STATES.index(s) for s in c['supported_states']] for c in result['cells']] == [sorted(s) for s in supported])
        check(name+': coordinates and uniqueness semantics', all(
            c['id'] == v and c['position'] == [v//4,(v//2)%2,v%2] and
            c['unique_state'] == (STATES[next(iter(supported[v]))] if len(supported[v]) == 1 else None)
            for v,c in enumerate(result['cells'])))
        check(name+': joint uniqueness correct', result['joint_solution_unique'] == (len(worlds)==1 if worlds else None))
        check(name+': complete original metadata preserved', result['metadata'] == metadata)
        check(name+': every PNG sample mapped to correct label', result['observations'] == dict(
            pixels=len(pixels), background=pixels.count(COLORS[0]),stone=pixels.count(COLORS[1]),oak=pixels.count(COLORS[2])))
        if worlds:
            witness = tuple(STATES.index(s) for s in result['witness']['states'])
            check(name+': simultaneous witness is an enumerated solution', witness in worlds)
            check(name+': witness satisfies every original image pixel independently',
                  all(sample(h,witness,metadata['palette']) == target for h,target in constraints))
        else:
            check(name+': infeasibility has no substituted scene', result['witness'] is None and all(not s for s in supported))
        check(name+': legal Minecraft palette mapping',
              result['state_catalog']['bottom_slab'] == {'block':'minecraft:oak_slab','properties':{'type':'bottom','waterlogged':'false'}} and
              result['state_catalog']['top_slab'] == {'block':'minecraft:stone_slab' if metadata['palette']=='split_material' else 'minecraft:oak_slab',
                                                      'properties':{'type':'top','waterlogged':'false'}})
        return worlds

    with tempfile.TemporaryDirectory(prefix='mcr-png-e2e-') as directory:
        tmp = Path(directory); caches = {}
        for name in ('visible-mixed','occluded-oak'):
            example = args.examples/name
            metadata = json.loads((example/'camera.json').read_text())
            scene = json.loads((example/'scene.json').read_text())
            width,height,pixels = read_png(example/'image.png')
            truth = tuple(STATES.index(s) for s in scene['states'])
            check(name+': committed PNG is the independent rendering of committed truth',
                  (width,height) == (metadata['image']['width'],metadata['image']['height']) and pixels == render(metadata,truth))
            output = tmp/(name+'.json')
            run = invoke(example/'image.png',example/'camera.json',output)
            check(name+': actual file-based CLI succeeds', run.returncode == 0 and run.stdout == 'SAT\n')
            first = output.read_bytes(); result = json.loads(first)
            check(name+': committed reconstruction reproduced exactly', first == (example/'reconstruction.json').read_bytes())
            again = invoke(example/'image.png',example/'camera.json',output)
            check(name+': identical inputs give byte-identical output', again.returncode == 0 and output.read_bytes() == first)
            worlds = verify(name,metadata,pixels,result)
            check(name+': original truth belongs to feasible family', truth in worlds)
            if name == 'visible-mixed':
                check('asymmetric oblique view uniquely recovers both materials', len(worlds) == 1 and COLORS[1] in pixels and COLORS[2] in pixels)
            else:
                check('occlusion ambiguity remains explicit: 81 scenes, four hidden cells',
                      len(worlds) == 81 and [c['id'] for c in result['cells'] if c['unique_state'] is None] == [0,1,4,5])
                check('chosen occluded witness differs from generating truth without claiming recovery',
                      result['witness']['states'] != scene['states'] and 'not a claim' in result['witness_semantics'])
            caches[name] = metadata,pixels,result
            # The same two scenes/cameras with the other supported palette.
            paired = copy.deepcopy(metadata); paired['palette'] = 'same_material' if metadata['palette']=='split_material' else 'split_material'
            paired_pixels = render(paired,truth)
            camera = tmp/'paired.json'; image = tmp/'paired.png'; camera.write_text(json.dumps(paired))
            image.write_bytes(png_bytes(width,height,paired_pixels))
            run = invoke(image,camera,output)
            check(name+': opposite palette runs from PNG bytes', run.returncode == 0)
            verify(name+'-other-palette',paired,paired_pixels,json.loads(output.read_text()))

        metadata,pixels,baseline = caches['visible-mixed']; width = height = 32
        camera = tmp/'camera.json'; image = tmp/'input.png'; output = tmp/'output.json'
        camera.write_text(json.dumps(metadata))
        rgb = png_bytes(width,height,pixels)
        for label,encoded in (
            ('all five PNG row filters',png_bytes(width,height,pixels,filters=(0,1,2,3,4))),
            ('opaque RGBA',png_bytes(width,height,[p+(255,) for p in pixels],alpha=True,filters=(4,3,2,1,0))),
            ('gamma metadata leaves exact sample labels unchanged',png_bytes(width,height,pixels,extra_chunks=((b'gAMA',struct.pack('>I',45455)),)))):
            image.write_bytes(encoded); run = invoke(image,camera,output)
            check(label,run.returncode == 0 and json.loads(output.read_text()) == baseline)
        idat_len = struct.unpack('>I',rgb[33:37])[0]; compressed = rgb[41:41+idat_len]
        image.write_bytes(rgb[:33]+chunk(b'IDAT',compressed[:5])+chunk(b'IDAT',compressed[5:])+chunk(b'IEND',b''))
        run = invoke(image,camera,output)
        check('consecutive split IDAT chunks decode identically',run.returncode == 0 and json.loads(output.read_text()) == baseline)

        bad_pixels = list(pixels); bad_pixels[0] = COLORS[1]
        image.write_bytes(png_bytes(width,height,bad_pixels)); run = invoke(image,camera,output)
        check('valid color on an escaping ray returns explicit UNSAT',run.returncode == 0 and run.stdout == 'UNSAT\n')
        verify('impossible-foreground',metadata,bad_pixels,json.loads(output.read_text()))
        same = copy.deepcopy(metadata); same['palette'] = 'same_material'; camera.write_text(json.dumps(same))
        image.write_bytes(rgb); run = invoke(image,camera,output)
        check('stone observation in oak-only palette is UNSAT, not a substituted scene',run.returncode == 0 and run.stdout == 'UNSAT\n')
        verify('stone-in-oak-palette',same,pixels,json.loads(output.read_text()))

        # A central ray crosses the bottoms of cells 0 and 4. Either can supply
        # the oak hit: every marginal state is supported, but all-air is invalid.
        single = copy.deepcopy(metadata)
        single['image'].update(width=1,height=1)
        single['camera'] = dict(center=[-1,'1/4','1/2'],forward=[1,0,0],right=[0,0,-1],up=[0,1,0],fx=1,fy=1,cx='1/2',cy='1/2')
        camera.write_text(json.dumps(single)); image.write_bytes(png_bytes(1,1,[COLORS[2]])); run = invoke(image,camera,output)
        check('analytical central-ray case executes',run.returncode == 0)
        result = json.loads(output.read_text()); worlds = verify('coupled-single-ray',single,[COLORS[2]],result)
        check('marginals are not a Cartesian-product scene',len(worlds) == 3645 and all(len(c['supported_states']) == 3 for c in result['cells']) and (0,)*8 not in worlds)

        camera.write_text(json.dumps(metadata)); image.write_bytes(rgb)
        def reject(name, *, png=None, meta=None, args_extra=(), expected=2):
            image.write_bytes(rgb if png is None else png)
            camera.write_bytes(json.dumps(metadata).encode() if meta is None else meta if isinstance(meta,bytes) else json.dumps(meta).encode())
            output.write_bytes(b'previous-output-sentinel\n')
            run = invoke(image,camera,output,extra=args_extra)
            check(name,run.returncode == expected and bool(run.stderr) and output.read_bytes() == b'previous-output-sentinel\n')

        invalid_images = {
            'PNG signature rejected':b'not a png',
            'truncated PNG rejected':rgb[:-1],
            'PNG without IEND rejected':rgb[:-12],
            'PNG trailing bytes rejected':rgb+b'x',
            'PNG CRC corruption rejected':rgb[:45]+bytes([rgb[45]^1])+rgb[46:],
            'duplicate IHDR rejected':rgb[:33]+rgb[8:33]+rgb[33:],
            'PNG chunk length overrun rejected':rgb[:33]+struct.pack('>I',0xffffffff)+rgb[37:],
            'nonconsecutive IDAT rejected':rgb[:33]+chunk(b'IDAT',compressed[:5])+chunk(b'tEXt',b'k\0v')+chunk(b'IDAT',compressed[5:])+chunk(b'IEND',b''),
            'APNG rejected':png_bytes(width,height,pixels,extra_chunks=((b'acTL',struct.pack('>II',1,0)),)),
            'unknown critical chunk rejected':png_bytes(width,height,pixels,extra_chunks=((b'ABCD',b''),)),
            'invalid PNG chunk name rejected':png_bytes(width,height,pixels,extra_chunks=((b'te1t',b''),)),
            'oversized input file rejected':b'x'*(1024*1024+1),
            'unsupported RGB value rejected':png_bytes(width,height,[(1,2,3)]+pixels[1:]),
            'transparent RGBA rejected':png_bytes(width,height,[pixels[0]+(0,)]+[p+(255,) for p in pixels[1:]],alpha=True),
            'bad deflate stream rejected':rgb[:33]+chunk(b'IDAT',b'broken')+chunk(b'IEND',b''),
            'invalid PNG row filter rejected':rgb[:33]+chunk(b'IDAT',zlib.compress(bytes([5])+zlib.decompress(compressed)[1:]))+chunk(b'IEND',b''),
        }
        for label,fields in [('dimensions', (31,32,8,2,0,0,0)),('16-bit samples',(32,32,16,2,0,0,0)),
                             ('indexed samples',(32,32,8,3,0,0,0)),('interlace',(32,32,8,2,0,0,1)),('huge dimensions',(0xffffffff,32,8,2,0,0,0))]:
            invalid_images[label+' rejected'] = rgb[:8]+chunk(b'IHDR',struct.pack('>IIBBBBB',*fields))+rgb[33:]
        for name,png in invalid_images.items(): reject(name,png=png)
        def changed(path,value):
            result = copy.deepcopy(metadata); parent = result
            for key in path[:-1]: parent = parent[key]
            parent[path[-1]] = value
            return result
        mutations = [
            ('unknown schema',['schema'],'unknown'),('unsupported palette',['palette'],'all_blocks'),
            ('unsupported grid',['grid','shape'],[3,2,2]),('unsupported origin',['grid','origin'],[1,0,0]),
            ('unsupported cell size',['grid','cell_size'],2),('image too large',['image','width'],65),
            ('zero dimensions',['image','height'],0),('wrong origin convention',['image','origin'],'bottom_left'),
            ('negative focal length',['camera','fx'],-1),('noninteger numeric parameter',['camera','fy'],1.5),
            ('zero rational denominator',['camera','cx'],'1/0'),('huge rational',['camera','fx'],'999999999999999999999999'),
            ('nonorthonormal basis',['camera','forward'],[1,1,0]),('left-handed basis',['camera','right'],['-1/3','-2/3','2/3']),
            ('wrong vector length',['camera','center'],[1,2]),('boolean integer',['image','height'],True),
            ('wrong encoding',['pixel_encoding'],'sRGB_nearest'),('unknown truth field',['truth'],[0]*8)]
        for name,path,value in mutations: reject(name+' rejected',meta=changed(path,value))
        missing = copy.deepcopy(metadata); del missing['camera']; reject('missing camera rejected',meta=missing)
        raw = json.dumps(metadata).encode()
        for name,meta in {
            'JSON duplicate root key rejected':b'{"schema":"wrong",'+raw[1:],
            'JSON duplicate nested key rejected':raw.replace(b'"fx": 48',b'"fx": 48, "fx": 48'),
            'JSON leading zero rejected':raw.replace(b'"fx": 48',b'"fx": 048'),
            'decimal rounding cannot silently alter camera':raw.replace(b'"fx": 48',b'"fx": 48.000000000000000000001'),
            'exponent underflow cannot silently alter camera':raw.replace(b'"fx": 48',b'"fx": 1e-999'),
            'JSON malformed rejected':b'{"camera":',
            'JSON trailing data rejected':raw+b' {}',
            'JSON excessive nesting rejected':b'['*32+b'0'+b']'*32,
            'JSON oversized file rejected':b' '*65537,
            'JSON non-ASCII rejected':raw+b'\xff',
            'JSON raw control character rejected':raw.replace(b'mcr-a2-camera-v1',b'mcr-a2-camera-\nv1'),
        }.items(): reject(name,meta=meta)
        reject('duplicate CLI option rejected',args_extra=('--image',str(image)))
        reject('unknown CLI option rejected',args_extra=('--guess','yes'))
        image.write_bytes(rgb); camera.write_bytes(raw)
        check('output cannot overwrite input image',invoke(image,camera,image).returncode == 2 and image.read_bytes() == rgb)
        check('output cannot overwrite metadata',invoke(image,camera,camera).returncode == 2 and camera.read_bytes() == raw)
        missing = invoke(tmp/'absent.png',camera,output)
        check('missing image explicitly rejected',missing.returncode == 2 and bool(missing.stderr))
        output.unlink(); run = invoke(image,camera,tmp/'missing-parent'/'result.json')
        check('output I/O failure is nonzero, not infeasibility',run.returncode == 3 and not (tmp/'missing-parent'/'result.json').exists())

    report = dict(schema='mcr-png-demo-validation-v1',result='PASS',checks=checks,
                  executable_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),cli_invocations=calls,
                  exhaustive_cases=enumerations,oracle='Independent Python Fraction camera rays, primitive AABB intersections, all 6561 legal worlds per case',
                  historical_experiments_run=False,larger_volume_collection_run=False)
    args.evidence.parent.mkdir(parents=True,exist_ok=True)
    args.evidence.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'result':'PASS','checks':len(checks),'cli_invocations':calls,'exhaustive_image_cases':len(enumerations)}))


if __name__ == '__main__':
    main()
