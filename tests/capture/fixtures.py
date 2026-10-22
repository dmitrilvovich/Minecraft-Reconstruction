"""Independent synthetic fixtures, NOT Minecraft screenshots.

Forward-rasterize world-space slab faces as triangles with a float z-buffer.
Does not use the diagnostic's local rational rays, boxes or intersection routine.
"""
import math
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'png_demo'))
from oracle import png_bytes

RGB=((135,206,235),(128,128,128),(160,112,64))


def raster(blocks,palette='split_material',width=640,height=480,eye=(1,81,14),focal=240):
    pixels=[RGB[0]]*(width*height); depths=[float('inf')]*(width*height)
    for block in blocks:
        state=block['state']
        if state=='air':continue
        x,y,z=block['game_position']; y+=0.5 if state=='top_slab' else 0
        # Game yaw=180: right=+X, up=+Y, forward=-Z.
        vertices=[]
        for n in range(8):
            wx=x+(n&1); wy=y+0.5*((n>>1)&1); wz=z+((n>>2)&1)
            depth=eye[2]-wz
            vertices.append((width/2+focal*(wx-eye[0])/depth,height/2-focal*(wy-eye[1])/depth,depth))
        color=RGB[1 if state=='top_slab' and palette=='split_material' else 2]
        for face in ((0,1,3,2),(4,5,7,6),(0,1,5,4),(2,3,7,6),(0,2,6,4),(1,3,7,5)):
            for tri in ((face[0],face[1],face[2]),(face[0],face[2],face[3])):
                a,b,c=[vertices[i] for i in tri]
                denom=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
                if abs(denom)<1e-15:continue
                for row in range(max(0,math.floor(min(a[1],b[1],c[1]))),min(height,math.ceil(max(a[1],b[1],c[1])))):
                    for col in range(max(0,math.floor(min(a[0],b[0],c[0]))),min(width,math.ceil(max(a[0],b[0],c[0])))):
                        px,py=col+0.5,row+0.5
                        u=((b[1]-c[1])*(px-c[0])+(c[0]-b[0])*(py-c[1]))/denom
                        v=((c[1]-a[1])*(px-c[0])+(a[0]-c[0])*(py-c[1]))/denom; t=1-u-v
                        if min(u,v,t)<-1e-12:continue
                        depth=1/(u/a[2]+v/b[2]+t/c[2]); i=row*width+col
                        if depth<depths[i]:depths[i]=depth;pixels[i]=color
    return pixels


def save(path,pixels,width=640,height=480,**kwargs):
    Path(path).write_bytes(png_bytes(width,height,pixels,**kwargs))
