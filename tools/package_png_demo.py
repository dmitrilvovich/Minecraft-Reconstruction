"""Package the tested Linux CLI and dependency notices deterministically."""
import argparse
import gzip
import io
from pathlib import Path
import tarfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    files = {'mcr_reconstruct': args.binary.read_bytes()}
    for library,file in [('LodePNG','third_party/lodepng/lodepng.cpp'),('PicoJSON','third_party/picojson/picojson.h')]:
        source = (ROOT/file).read_text()
        files['LICENSES/'+library+'.txt'] = source[:source.index('*/')+2].encode()+b'\n'
    files['README.txt'] = (
        'mcr_reconstruct: tested Linux x86-64 ELF, dynamically linked to glibc/libstdc++.\n'
        'See docs/png-reconstruction-demo.md in dmitrilvovich/Minecraft-Reconstruction.\n'
        'Usage: ./mcr_reconstruct --image image.png --camera camera.json --output reconstruction.json\n'
        'This is the PNG application, not the suspended scaling study worker.\n'
        'Source builds are canonical for other environments. Preserve LICENSES on redistribution.\n').encode()
    stream = io.BytesIO()
    with tarfile.open(fileobj=stream,mode='w',format=tarfile.USTAR_FORMAT) as archive:
        for name,data in sorted(files.items()):
            info = tarfile.TarInfo(name); info.size = len(data); info.mode = 0o755 if name == 'mcr_reconstruct' else 0o644
            info.mtime = info.uid = info.gid = 0
            archive.addfile(info,io.BytesIO(data))
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes(gzip.compress(stream.getvalue(),mtime=0))


if __name__ == '__main__':
    main()
