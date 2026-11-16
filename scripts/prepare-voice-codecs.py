#!/usr/bin/env python3
"""Fetch pinned Xiph source archives; all compilation remains offline."""
import hashlib, subprocess, urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]/'build/voice-codecs'
PACKAGES=[('opus','opus-1.6.1.tar.gz','6ffcb593207be92584df15b32466ed64bbec99109f007c82205f0194572411a1'),('opus','opusfile-0.12.tar.gz','118d8601c12dd6a44f52423e68ca9083cc9f2bfe72da7a8c1acb22a80ae3550b'),('ogg','libogg-1.3.5.tar.xz','c4d91be36fc8e54deae7575241e03f4211eb102afb3fc0775fbbc1b740016705')]
for group,name,digest in PACKAGES:
    archive=ROOT/'downloads'/name;archive.parent.mkdir(parents=True,exist_ok=True)
    if not archive.exists():
        with urllib.request.urlopen('https://downloads.xiph.org/releases/'+group+'/'+name,timeout=60) as r:archive.write_bytes(r.read())
    if hashlib.sha256(archive.read_bytes()).hexdigest()!=digest:raise RuntimeError('Unexpected codec source checksum: '+name)
    source=ROOT/'source';source.mkdir(exist_ok=True)
    folder=name.removesuffix('.tar.gz').removesuffix('.tar.xz')
    if not (source/folder/'configure').exists():subprocess.run(['tar','-xf',str(archive),'-C',str(source)],check=True)
