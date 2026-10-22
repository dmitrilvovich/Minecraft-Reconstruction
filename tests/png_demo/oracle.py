"""Independent tiny-grid file oracle. No solver, traversal or frozen-reference imports.

Camera arithmetic and primitive half-open AABB intersections use Fraction.
Enumeration visits all 3**8 worlds, rather than accepting configuration agreement.
The small PNG reader/writer uses only Python's standard library.
"""
from fractions import Fraction as Q
from itertools import product
import binascii
import struct
import zlib

COLORS = ((135, 206, 235), (128, 128, 128), (160, 112, 64))
STATES = ('air', 'bottom_slab', 'top_slab')


def chunk(kind, payload):
    return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', binascii.crc32(kind + payload) & 0xffffffff)


def png_bytes(width, height, pixels, *, alpha=False, filters=(0,), extra_chunks=()):
    """Encode actual RGB/RGBA samples with optional filter variants for tests."""
    bpp = 4 if alpha else 3
    rows = []; previous = bytes(width * bpp)
    for y in range(height):
        raw = bytes(v for pixel in pixels[y*width:(y+1)*width] for v in pixel)
        assert len(raw) == width*bpp
        method = filters[y % len(filters)]
        filtered = bytearray()
        for x, v in enumerate(raw):
            left = raw[x-bpp] if x >= bpp else 0
            up = previous[x]
            corner = previous[x-bpp] if x >= bpp else 0
            predictors = (0, left, up, (left+up)//2, paeth(left, up, corner))
            filtered.append((v-predictors[method]) & 255)
        rows.append(bytes([method])+filtered); previous = raw
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6 if alpha else 2, 0, 0, 0)) +
            b''.join(chunk(k,v) for k,v in extra_chunks) + chunk(b'IDAT', zlib.compress(b''.join(rows), 9)) + chunk(b'IEND', b''))


def paeth(a, b, c):
    p = a+b-c
    distances = (abs(p-a), abs(p-b), abs(p-c))
    return (a,b,c)[distances.index(min(distances))]


def read_png(path):
    data = path.read_bytes(); assert data[:8] == b'\x89PNG\r\n\x1a\n'
    pos = 8; compressed = bytearray(); width = height = bpp = None
    while pos < len(data):
        n = struct.unpack('>I',data[pos:pos+4])[0]; kind = data[pos+4:pos+8]; payload = data[pos+8:pos+8+n]
        assert binascii.crc32(kind+payload) & 0xffffffff == struct.unpack('>I',data[pos+8+n:pos+12+n])[0]
        if kind == b'IHDR':
            width,height,depth,color,compression,filtering,interlace = struct.unpack('>IIBBBBB',payload)
            assert depth == 8 and color in (2,6) and compression == filtering == interlace == 0
            bpp = 3 if color == 2 else 4
        if kind == b'IDAT': compressed.extend(payload)
        pos += n+12
        if kind == b'IEND': break
    raw = zlib.decompress(compressed); assert len(raw) == height*(1+width*bpp)
    pixels = []; previous = bytes(width*bpp)
    for y in range(height):
        start = y*(1+width*bpp); method = raw[start]; row = bytearray(raw[start+1:start+1+width*bpp])
        assert 0 <= method <= 4
        for x, v in enumerate(row):
            left = row[x-bpp] if x >= bpp else 0; up = previous[x]; corner = previous[x-bpp] if x >= bpp else 0
            row[x] = (v+(0,left,up,(left+up)//2,paeth(left,up,corner))[method]) & 255
        for x in range(width):
            p = tuple(row[x*bpp:(x+1)*bpp]); assert bpp == 3 or p[3] == 255
            pixels.append(p[:3])
        previous = row
    return width,height,pixels


def ray(metadata, column, row):
    c = metadata['camera']
    origin = tuple(map(Q,c['center']))
    u = (Q(column)+Q(1,2)-Q(c['cx']))/Q(c['fx'])
    v = (Q(c['cy'])-Q(row)-Q(1,2))/Q(c['fy'])
    direction = tuple(Q(f)+u*Q(r)+v*Q(up) for f,r,up in zip(c['forward'],c['right'],c['up']))
    return origin,direction


def hit(origin, direction, lo, hi):
    enter = Q(0); leave = None
    for o,d,a,b in zip(origin,direction,lo,hi):
        if d == 0:
            if not a <= o < b: return None
        else:
            t0,t1 = sorted(((a-o)/d,(b-o)/d))
            enter = max(enter,t0); leave = t1 if leave is None else min(leave,t1)
    return enter if leave is not None and enter < leave else None


def hits(metadata, column, row):
    origin,direction = ray(metadata,column,row); result = []
    for cell in range(8):
        x,y,z = cell//4,(cell//2)%2,cell%2
        for state in (1,2):
            lo = (Q(x),Q(y)+(Q(1,2) if state == 2 else 0),Q(z))
            hi = (Q(x+1),Q(y)+(Q(1,2) if state == 1 else 1),Q(z+1))
            depth = hit(origin,direction,lo,hi)
            if depth is not None: result.append((depth,cell,state))
    return tuple((v,s) for _,v,s in sorted(result))


def label(state, palette):
    return 0 if state == 0 else 1 if state == 2 and palette == 'split_material' else 2


def sample(hits_, world, palette):
    for cell,state in hits_:
        if world[cell] == state: return label(state,palette)
    return 0


def render(metadata, world):
    im = metadata['image']
    return [COLORS[sample(hits(metadata,c,r),world,metadata['palette'])]
            for r in range(im['height']) for c in range(im['width'])]


def enumerate_image(metadata, pixels):
    im = metadata['image']; assert len(pixels) == im['width']*im['height']
    constraints = [(hits(metadata,c,r),COLORS.index(pixels[r*im['width']+c]))
                   for r in range(im['height']) for c in range(im['width'])]
    # Equivalent raw AABB hit lists are a reference-only evaluation shortcut.
    # No case, truth world or observation is dropped or supplied to the solver.
    distinct = list(dict.fromkeys(constraints))
    worlds = []; supported = [set() for _ in range(8)]
    for world in product(range(3),repeat=8):
        if all(sample(h,world,metadata['palette']) == target for h,target in distinct):
            worlds.append(world)
            for v,s in enumerate(world): supported[v].add(s)
    return worlds,supported,constraints
