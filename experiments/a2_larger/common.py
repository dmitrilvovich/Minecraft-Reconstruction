"""Frozen-protocol utilities and closed, verifiable, atomic archive storage."""
from __future__ import annotations
import gzip
import hashlib
import json
import os
from pathlib import Path
import shutil
import time

ROOT = Path(__file__).resolve().parents[2]
PROTOCOL_PATH = ROOT / 'experiments/protocols/a2-scaling-larger-v1.json'
PROTOCOL = json.loads(PROTOCOL_PATH.read_text())
SEED = PROTOCOL['determinism']['master_seed']
BINARY = ROOT / 'build/a2-larger/worker'
PALETTES = PROTOCOL['camera_truths']['palettes']
MODES = [x['id'] for x in PROTOCOL['freeze']['configurations']]

def canonical(value):
    return json.dumps(value, ensure_ascii=True, sort_keys=True, separators=(',', ':'), allow_nan=False)

def H(label, *args):
    return int.from_bytes(hashlib.sha256(canonical([SEED, label, *args]).encode('ascii')).digest(), 'big')

def digest(value):
    return hashlib.sha256(canonical(value).encode('ascii')).hexdigest()

def file_hash(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for data in iter(lambda: f.read(1 << 20), b''):
            h.update(data)
    return h.hexdigest()

def sync_dir(path):
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)

def atomic_json(path, value):
    path = Path(path); path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + '.tmp')
    with tmp.open('w', encoding='ascii', newline='\n') as f:
        f.write(canonical(value) + '\n'); f.flush(); os.fsync(f.fileno())
    os.replace(tmp, path); sync_dir(path.parent)

def read_records(path):
    with gzip.open(path, 'rt', encoding='ascii') as f:
        for line in f:
            obj = json.loads(line)
            if '_archive' not in obj:
                yield obj

def inspect_archive(path, expected_context=None):
    """Checks closed header/footer, line count, both representations and ordering digest."""
    path = Path(path); raw_hash = hashlib.sha256(); keys = hashlib.sha256(); count = size = 0
    header = footer = None; seen=set()
    with gzip.open(path, 'rb') as f:
        for line in f:
            if not line.endswith(b'\n'):
                raise ValueError('Truncated JSONL record')
            raw_hash.update(line); size += len(line); obj = json.loads(line)
            if header is None:
                if obj.get('_archive') != 'header':
                    raise ValueError('Missing archive header')
                header = obj['context']; continue
            if footer is not None:
                raise ValueError('Data after footer')
            if obj.get('_archive') == 'footer':
                footer = obj; continue
            if '_archive' in obj:
                raise ValueError('Unexpected archive marker')
            if 'record_id' in obj:
                if obj['record_id'] in seen: raise ValueError('Duplicate record ID')
                seen.add(obj['record_id'])
            keys.update(canonical(obj.get('record_id', count)).encode('ascii') + b'\n'); count += 1
    if footer is None or footer['records'] != count or footer['keys_sha256'] != keys.hexdigest():
        raise ValueError('Incomplete/invalid archive')
    if expected_context is not None and header != expected_context:
        raise ValueError('Archive provenance mismatch')
    if header.get('expected_keys_sha256') is not None and keys.hexdigest()!=header['expected_keys_sha256']:
        raise ValueError('Records differ from the declared ordered calls')
    return dict(file=path.name, records=count, raw_bytes=size, bytes=path.stat().st_size,
                sha256=file_hash(path), raw_sha256=raw_hash.hexdigest(), keys_sha256=keys.hexdigest(), context=header)

def verify_archive(path, expected=None, expected_context=None):
    actual = inspect_archive(path, expected_context)
    if expected is not None and actual != expected:
        raise ValueError('Archive hash/metadata mismatch: ' + str(path))
    return actual

def write_archive(path, rows, context, expected_count=None):
    """Publish only after close/fsync, record validation and decompression hash check.

    A sealed orphan can recover its completion marker without executing rows.
    An unsealed partial is preserved and blocks automatic recovery/re-execution.
    """
    path = Path(path); path.parent.mkdir(parents=True, exist_ok=True)
    marker = path.with_name(path.name + '.complete.json')
    if marker.exists():
        actual = verify_archive(path, json.loads(marker.read_text()), context)
        if expected_count is not None and actual['records'] != expected_count:
            raise ValueError('Cached record count differs')
        return actual
    if path.exists():
        actual = inspect_archive(path, context)
        if expected_count is not None and actual['records'] != expected_count:
            raise ValueError('Orphan archive count differs')
        atomic_json(marker, actual); return actual
    raw = path.with_name(path.name + '.partial.jsonl')
    packed = path.with_name(path.name + '.partial.gz')
    if raw.exists() or packed.exists():
        raise RuntimeError('Interrupted artifact preserved; review required before recovery: ' + str(path))
    count = 0; keys = hashlib.sha256()
    with raw.open('xb') as f:
        f.write((canonical({'_archive':'header','context':context}) + '\n').encode('ascii'))
        for obj in rows:
            if '_archive' in obj:
                raise ValueError('Reserved archive field')
            f.write((canonical(obj) + '\n').encode('ascii'))
            keys.update(canonical(obj.get('record_id',count)).encode('ascii') + b'\n'); count += 1
            # Valid records are recoverable even if the process dies before sealing.
            f.flush()
        if expected_count is not None and count != expected_count:
            raise ValueError(f'Expected {expected_count} records, got {count}')
        f.write((canonical({'_archive':'footer','records':count,'keys_sha256':keys.hexdigest()})+'\n').encode('ascii'))
        f.flush(); os.fsync(f.fileno())
    original_hash = file_hash(raw)
    with packed.open('xb') as target:
        with gzip.GzipFile(filename='', mode='wb', fileobj=target, compresslevel=9, mtime=0) as z:
            with raw.open('rb') as source:
                shutil.copyfileobj(source, z, 1 << 20)
        target.flush(); os.fsync(target.fileno())
    verified = inspect_archive(packed, context)
    if verified['raw_sha256'] != original_hash or verified['records'] != count:
        raise ValueError('Decompression verification failed')
    os.replace(packed, path); sync_dir(path.parent)
    verified['file'] = path.name
    atomic_json(marker, verified)
    raw.unlink(); sync_dir(path.parent)
    return verified

def protocol_identity():
    return {'protocol_sha256': file_hash(PROTOCOL_PATH), 'binary_sha256': file_hash(BINARY)}

def source_identity():
    return {p.relative_to(ROOT).as_posix():file_hash(p) for p in sorted((ROOT/'experiments/a2_larger').iterdir())
            if p.is_file() and p.suffix in ('.py','.cpp','.sh','.json')}

class AdministrativeBudget:
    def __init__(self, path):
        self.path = Path(path)
        self.state = json.loads(self.path.read_text()) if self.path.exists() else {'preparation_seconds':0.,'solver_seconds':0.}

    def add(self, kind, seconds):
        key = kind + '_seconds'; self.state[key] += seconds
        atomic_json(self.path, self.state)
        limit = 12*3600 if kind == 'preparation' else 48*3600
        if self.state[key] >= limit:
            raise RuntimeError('Administrative pause: ' + kind)

    def reserve(self, output):
        if shutil.disk_usage(output).free < 20 * 1024**3:
            raise RuntimeError('Need declared 20 GiB storage reserve before collection')
