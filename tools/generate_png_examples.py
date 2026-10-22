"""Reproduce the two small demonstration PNGs and run the actual CLI on them.

Standard-library rational AABB rendering is independent of C++ inference.
This utility never reads or executes any research protocol or collector.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tests/png_demo'))
from oracle import STATES, png_bytes, render


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--examples', type=Path, default=ROOT/'examples/a2-png')
    args = parser.parse_args()
    for name in ('visible-mixed','occluded-oak'):
        directory = args.examples/name
        metadata = json.loads((directory/'camera.json').read_text())
        scene = json.loads((directory/'scene.json').read_text())
        pixels = render(metadata,tuple(STATES.index(s) for s in scene['states']))
        im = metadata['image']
        (directory/'image.png').write_bytes(png_bytes(im['width'],im['height'],pixels))
        subprocess.run([str(args.binary.resolve()),'--image',str(directory/'image.png'),
                        '--camera',str(directory/'camera.json'),'--output',str(directory/'reconstruction.json')],check=True)
        print(name + ': PNG and reconstruction written',flush=True)


if __name__ == '__main__':
    main()
