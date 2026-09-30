#!/usr/bin/env python3
"""Lossless columnar transport of CSVs; verifies exact CSV-byte round trips.

pack RAW OUTPUT; unpack OUTPUT RAW_COPY. No solver is invoked.
The archive includes every original job manifest, log, spot case, and raw table.
"""
import argparse
import csv
import gzip
import hashlib
import io
import json
from pathlib import Path
import shutil
import tarfile
import tempfile
import numpy as np
import pandas as pd


def sha(path):
    with open(path, 'rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def decode(npz, out):
    with np.load(npz, allow_pickle=False) as data:
        schema = json.loads(data['schema'].tobytes())
        columns = []
        for i, col in enumerate(schema['columns']):
            values = data['c'+str(i)]
            if 'dictionary' in col:
                values = np.array(col['dictionary'])[values]
            columns.append(values)
        writer = csv.writer(out, lineterminator='\n')
        writer.writerow([c['name'] for c in schema['columns']])
        for start in range(0, schema['rows'], 8192):
            writer.writerows(zip(*(c[start:start+8192].tolist() for c in columns)))
        return schema


def encode(src, dest):
    frame = pd.read_csv(src, keep_default_na=False)
    schema = dict(rows=len(frame), columns=[])
    arrays = {}
    for i, name in enumerate(frame):
        col = dict(name=name)
        values = frame[name].to_numpy()
        if values.dtype.kind not in 'iu':
            dictionary, values = np.unique(values.astype(str), return_inverse=True)
            col['dictionary'] = dictionary.tolist()
        low, high = int(values.min()), int(values.max())
        if low >= 0:
            dtype = np.min_scalar_type(high)
        else:
            dtype = next(np.dtype(t) for t in ['int8','int16','int32','int64'] if np.iinfo(t).min<=low and high<=np.iinfo(t).max)
        arrays['c'+str(i)] = values.astype(dtype)
        schema['columns'].append(col)
    arrays['schema'] = np.frombuffer(json.dumps(schema, separators=(',', ':')).encode(), dtype=np.uint8)
    np.savez_compressed(dest, **arrays)
    with gzip.open(src, 'rb') as f:
        original = hashlib.file_digest(f, 'sha256').hexdigest()
    # Compare actual reconstructed bytes, not only array values or row counts.
    with tempfile.TemporaryFile(mode='w+', encoding='utf-8', newline='') as f:
        decode(dest, f); f.flush(); f.seek(0)
        h = hashlib.sha256()
        for chunk in iter(lambda: f.read(1024*1024), ''):
            h.update(chunk.encode())
    assert h.hexdigest() == original, src
    return dict(npz=dest.name, npz_sha256=sha(dest), original_gzip_sha256=sha(src),
                csv_sha256=original, rows=len(frame), columns=len(frame.columns))


def pack(raw, output):
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='a2-transport-') as temporary:
        temp = Path(temporary)
        tables = {}
        files = {}
        protocol=json.loads((raw/'protocol.json').read_text())
        names={'protocol.json','provenance.json','collection.json'}
        for job in protocol['jobs']:
            names.update(job['name']+suffix for suffix in ['.json','.log','.cases.csv.gz','.runs.csv.gz','.spots.jsonl.gz'])
        # Only complete declared job files belong to the dataset, never stale .tmp files.
        for name in sorted(names):
            p=raw/name
            assert p.is_file(), name
            if p.name.endswith('.csv.gz'):
                dest = temp/(p.name[:-7]+'.npz')
                tables[p.name] = encode(p, dest)
            else:
                shutil.copyfile(p, temp/p.name)
            print('PACK', p.name, flush=True)
        for p in sorted(temp.iterdir()):
            files[p.name] = dict(sha256=sha(p), bytes=p.stat().st_size)
        (temp/'transport.json').write_text(json.dumps(dict(schema='MCR-A2-LOSSLESS-CSV-1',tables=tables,files=files),indent=2)+'\n')
        tar_path = temp/'raw.tar'
        with tarfile.open(tar_path, 'w', format=tarfile.USTAR_FORMAT) as tar:
            for p in sorted(temp.iterdir()):
                if p == tar_path:
                    continue
                info = tarfile.TarInfo(p.name); info.size=p.stat().st_size; info.mode=0o644; info.mtime=0
                with p.open('rb') as f:
                    tar.addfile(info, f)
        archive_sha = sha(tar_path)
        parts = []
        with tar_path.open('rb') as f:
            for index, data in enumerate(iter(lambda:f.read(2*1024*1024), b'')):
                name=f'raw.tar.part-{index:03d}'
                (output/name).write_bytes(data)
                parts.append(dict(name=name, bytes=len(data), sha256=sha(output/name)))
        h = hashlib.sha256()
        for part in parts:
            h.update((output/part['name']).read_bytes())
        assert h.hexdigest()==archive_sha
        manifest = dict(schema='MCR-A2-TRANSPORT-1',tar_sha256=archive_sha,bytes=tar_path.stat().st_size,
                        parts=parts, tables=len(tables), exact_csv_byte_roundtrips=len(tables),
                        records=sum(x['rows'] for x in tables.values()),
                        original_csv_gzip_bytes=sum((raw/p).stat().st_size for p in tables))
        (output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        print(json.dumps({k:v for k,v in manifest.items() if k!='parts'}),flush=True)


def unpack(source, output):
    manifest=json.loads((source/'manifest.json').read_text())
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='a2-unpack-') as temporary:
        temp=Path(temporary); tarpath=temp/'raw.tar'
        with tarpath.open('wb') as f:
            for part in manifest['parts']:
                p=source/part['name']; assert p.stat().st_size==part['bytes'] and sha(p)==part['sha256']
                with p.open('rb') as r:
                    shutil.copyfileobj(r,f)
        assert sha(tarpath)==manifest['tar_sha256']
        with tarfile.open(tarpath) as tar:
            for member in tar:
                assert member.isfile() and Path(member.name).name==member.name
                with tar.extractfile(member) as r, (temp/member.name).open('wb') as f:
                    shutil.copyfileobj(r,f)
        transport=json.loads((temp/'transport.json').read_text())
        for name, checks in transport['files'].items():
            assert sha(temp/name)==checks['sha256']
        for original, table in transport['tables'].items():
            raw=temp/original[:-3]
            with raw.open('w',newline='',encoding='utf-8') as f:
                decode(temp/table['npz'],f)
            assert sha(raw)==table['csv_sha256']
            with raw.open('rb') as r, (output/original).open('wb') as f:
                with gzip.GzipFile(filename='',fileobj=f,mode='wb',mtime=0,compresslevel=6) as z:
                    shutil.copyfileobj(r,z)
            # Original gzip encoding is restored with the recorded encoder.
            assert sha(output/original)==table['original_gzip_sha256']
        for name in transport['files']:
            if not name.endswith('.npz'):
                shutil.copyfile(temp/name,output/name)
    print('Restored all raw job files and verified CSV and gzip hashes')


if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('action',choices=['pack','unpack']);p.add_argument('source',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();(pack if a.action=='pack' else unpack)(a.source,a.output)
