"""Small stdlib-only capture IO and exact validation geometry; never calls inference."""
from fractions import Fraction as Q
from pathlib import Path
import binascii
import json
import re
import struct
import zlib

COLORS = ((135, 206, 235), (128, 128, 128), (160, 112, 64))
LABELS = ('background', 'stone', 'oak')
STATES = ('air', 'bottom_slab', 'top_slab')


def require(ok, message):
    if not ok:
        raise ValueError(message)


def json_read(path):
    require(Path(path).stat().st_size <= 65536, 'metadata exceeds 64 KiB')
    data = Path(path).read_bytes()
    require(len(data) <= 65536, 'metadata exceeds 64 KiB')
    def pairs(items):
        d = {}
        for k, v in items:
            require(k not in d, 'duplicate JSON key: ' + k)
            d[k] = v
        return d
    def no_float(value):
        raise ValueError('use integer tokens or rational strings, not ' + value)
    try:
        return json.loads(data, object_pairs_hook=pairs, parse_float=no_float, parse_constant=no_float)
    except RecursionError as exc:
        raise ValueError('metadata nesting too deep') from exc


def keys(value, expected):
    require(isinstance(value, dict) and set(value) == set(expected.split()), 'unexpected metadata fields: ' + expected)


def integer(value, low, high):
    require(type(value) is int and low <= value <= high, 'integer out of range')
    return value


def rational(value):
    require(type(value) is int or (isinstance(value, str) and re.fullmatch(r'-?\d+/[1-9]\d*', value)), 'invalid rational')
    if isinstance(value, str):
        a, b = map(int, value.split('/'))
    else:
        a, b = value, 1
    require(abs(a) <= 1000000 and b <= 1000000, 'rational exceeds core input bound')
    return Q(a, b)


def camera_read(path):
    m = json_read(path)
    keys(m, 'schema grid palette pixel_encoding image camera')
    require(m['schema'] == 'mcr-a2-camera-v1', 'unsupported camera schema')
    require(json.dumps(m['grid'],sort_keys=True) == json.dumps({'shape': [2,2,2], 'origin': [0,0,0], 'cell_size': 1},sort_keys=True), 'only unit 2x2x2 model grid supported')
    require(m['palette'] in ('split_material', 'same_material'), 'unsupported palette')
    require(m['pixel_encoding'] == 'mcr-a2-first-hit-rgb-v1', 'unsupported observation encoding')
    keys(m['image'], 'width height origin')
    for k in ('width', 'height'):
        integer(m['image'][k], 1, 64)
    require(m['image']['origin'] == 'top_left', 'image must use top-left origin')
    c = m['camera']; keys(c, 'center forward right up fx fy cx cy')
    for k in ('center', 'forward', 'right', 'up'):
        require(isinstance(c[k], list) and len(c[k]) == 3, 'camera vectors need three components')
        for v in c[k]: rational(v)
    f, r, u = [list(map(rational, c[k])) for k in ('forward', 'right', 'up')]
    dot = lambda a, b: sum(x*y for x,y in zip(a,b))
    cross = [r[1]*u[2]-r[2]*u[1], r[2]*u[0]-r[0]*u[2], r[0]*u[1]-r[1]*u[0]]
    require(all(dot(a,a) == 1 for a in (f,r,u)) and all(dot(a,b) == 0 for a,b in ((f,r),(f,u),(r,u))) and cross == f, 'invalid camera basis')
    for k in ('fx','fy','cx','cy'): rational(c[k])
    require(rational(c['fx']) > 0 and rational(c['fy']) > 0, 'nonpositive focal length')
    return m


def png_chunk(kind, payload):
    return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', binascii.crc32(kind+payload) & 0xffffffff)


def png_write(path, width, height, pixels):
    require(len(pixels) == width*height, 'wrong pixel count')
    raw = b''.join(b'\0' + bytes(c for p in pixels[y*width:(y+1)*width] for c in p) for y in range(height))
    Path(path).write_bytes(b'\x89PNG\r\n\x1a\n' + png_chunk(b'IHDR', struct.pack('>IIBBBBB', width,height,8,2,0,0,0)) + png_chunk(b'IDAT', zlib.compress(raw,9)) + png_chunk(b'IEND',b''))


def png_read(path):
    require(Path(path).stat().st_size <= 16*1024*1024, 'PNG file exceeds 16 MiB')
    data = Path(path).read_bytes()
    require(len(data) <= 16*1024*1024 and data[:8] == b'\x89PNG\r\n\x1a\n', 'invalid PNG or file exceeds 16 MiB')
    pos = 8; header = None; packed = bytearray(); ended = False; idat_closed = False; seen_idat = False; seen_plte = False
    while pos < len(data):
        require(pos+12 <= len(data), 'truncated PNG chunk')
        n = struct.unpack_from('>I',data,pos)[0]; kind = data[pos+4:pos+8]; end = pos+n+12
        require(end <= len(data) and re.fullmatch(b'[A-Za-z]{4}',kind) and not kind[2]&32, 'invalid PNG chunk')
        payload = data[pos+8:end-4]
        require(binascii.crc32(kind+payload)&0xffffffff == struct.unpack_from('>I',data,end-4)[0], 'PNG CRC mismatch')
        require(header is not None or kind == b'IHDR', 'IHDR must be first')
        require(kind not in (b'acTL',b'fcTL',b'fdAT',b'tRNS'), 'animation/transparency unsupported')
        if kind == b'IHDR':
            require(header is None and n == 13, 'invalid IHDR')
            w,h,depth,color,comp,filt,inter = struct.unpack('>IIBBBBB',payload)
            require(1 <= w <= 4096 and 1 <= h <= 4096 and w*h <= 4194304, 'PNG dimensions exceed diagnostic bound')
            require(depth == 8 and color in (2,6) and comp == filt == inter == 0, 'need noninterlaced RGB8 or opaque RGBA8')
            header = (w,h,3 if color == 2 else 4)
        elif kind == b'IDAT':
            require(not idat_closed, 'nonconsecutive IDAT')
            packed.extend(payload); seen_idat = True
        elif kind == b'IEND':
            require(n == 0 and seen_idat and end == len(data), 'invalid IEND/trailing bytes')
            ended = True; break
        elif kind == b'PLTE':
            require(not seen_plte and not seen_idat and 0 < n <= 768 and n%3 == 0, 'invalid PLTE')
            seen_plte = True
        else:
            if seen_idat: idat_closed = True
            require(kind[0]&32, 'unknown critical PNG chunk')
        pos = end
    require(ended, 'missing IEND')
    w,h,bpp = header; size = h*(1+w*bpp)
    decoder = zlib.decompressobj()
    try:
        raw = decoder.decompress(bytes(packed),size+1)
    except zlib.error as exc:
        raise ValueError('invalid PNG deflate data') from exc
    require(len(raw) == size and decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail, 'invalid/oversized PNG decompression')
    previous = bytes(w*bpp); pixels = []
    for y in range(h):
        start = y*(1+w*bpp); method = raw[start]; require(method <= 4, 'invalid PNG filter')
        row = bytearray(raw[start+1:start+1+w*bpp])
        for i in range(len(row)):
            a = row[i-bpp] if i >= bpp else 0; b = previous[i]; c = previous[i-bpp] if i >= bpp else 0
            p = a+b-c; dist = (abs(p-a),abs(p-b),abs(p-c)); paeth = (a,b,c)[dist.index(min(dist))]
            row[i] = (row[i] + (0,a,b,(a+b)//2,paeth)[method]) & 255
        for i in range(0,len(row),bpp):
            require(bpp == 3 or row[i+3] == 255, 'transparent pixel')
            pixels.append(tuple(row[i:i+3]))
        previous = row
    return w,h,pixels


def boxes(world):
    for cell, state in enumerate(world):
        if state:
            x,y,z = cell//4,(cell//2)%2,cell%2
            yield cell,state,(Q(x),Q(y)+(Q(1,2) if state == 2 else 0),Q(z)),(Q(x+1),Q(y)+(1 if state == 2 else Q(1,2)),Q(z+1))


def intersect(origin, direction, lo, hi):
    entry = Q(0); leave = None
    for o,d,a,b in zip(origin,direction,lo,hi):
        if not d:
            if not a <= o < b: return None
        else:
            t0,t1 = sorted(((a-o)/d,(b-o)/d)); entry = max(entry,t0)
            leave = t1 if leave is None else min(leave,t1)
    return entry if leave is not None and entry < leave else None


def render_truth(metadata, world):
    """Exact raw rational rays/AABBs, separate from C++ traversal and inference."""
    c = metadata['camera']; w,h = (metadata['image'][k] for k in ('width','height'))
    origin = list(map(rational,c['center'])); f,r,u = [list(map(rational,c[k])) for k in ('forward','right','up')]
    fx,fy,cx,cy = [rational(c[k]) for k in ('fx','fy','cx','cy')]; slabs = list(boxes(world))
    labels=[]; ids=[]
    for row in range(h):
        for col in range(w):
            a=(Q(col)+Q(1,2)-cx)/fx; b=(cy-Q(row)-Q(1,2))/fy
            direction=[f[i]+a*r[i]+b*u[i] for i in range(3)]
            hits=[]
            for v,s,lo,hi in slabs:
                t=intersect(origin,direction,lo,hi)
                if t is not None: hits.append((t,v,s))
            if hits:
                _,v,s=min(hits); labels.append(1 if s==2 and metadata['palette']=='split_material' else 2); ids.append(v)
            else: labels.append(0); ids.append(-1)
    return labels,ids


def boundary_mask(metadata, world, radius=1):
    """Conservative one-pixel band around every projected box edge, even hidden ones.

    Deliberately over-marks: this is a warning band, not a proof of raster agreement.
    Exact Fraction projection/distance avoids a floating tolerance decision.
    """
    c=metadata['camera']; w,h=(metadata['image'][k] for k in ('width','height'))
    origin=list(map(rational,c['center'])); f,r,u=[list(map(rational,c[k])) for k in ('forward','right','up')]
    fx,fy,cx,cy=[rational(c[k]) for k in ('fx','fy','cx','cy')]
    mask=[False]*(w*h)
    for _,_,lo,hi in boxes(world):
        corners=[]
        for n in range(8):
            p=[(hi[i] if n&(1<<i) else lo[i])-origin[i] for i in range(3)]
            z=sum(p[i]*f[i] for i in range(3))
            require(z > 0, 'truth must be entirely in front of the camera')
            corners.append((cx+fx*sum(p[i]*r[i] for i in range(3))/z,cy-fy*sum(p[i]*u[i] for i in range(3))/z))
        for a in range(8):
            for axis in range(3):
                b=a^(1<<axis)
                if b <= a: continue
                x0,y0=corners[a]; x1,y1=corners[b]; dx,dy=x1-x0,y1-y0; length=dx*dx+dy*dy
                for y in range(max(0,int(min(y0,y1))-radius-1),min(h,int(max(y0,y1))+radius+2)):
                    for x in range(max(0,int(min(x0,x1))-radius-1),min(w,int(max(x0,x1))+radius+2)):
                        px,py=Q(x)+Q(1,2)-x0,Q(y)+Q(1,2)-y0
                        t=max(Q(0),min(Q(1),(px*dx+py*dy)/length)) if length else Q(0)
                        if (px-t*dx)**2+(py-t*dy)**2 <= radius*radius: mask[y*w+x]=True
    return mask
