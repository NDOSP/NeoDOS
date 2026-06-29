#!/usr/bin/env python3
import sys
import os
from pyfatfs.PyFatFS import PyFatFS

img = sys.argv[1]
pairs = [a.split('=', 1) for a in sys.argv[2:]]

fs = PyFatFS(img)

for src, dst in pairs:
    dstdir = os.path.dirname(dst)
    if dstdir:
        parts = dstdir.split('/')
        path = ''
        for p in parts:
            path += '/' + p
            if not fs.exists(path):
                fs.makedir(path)
    dstpath = '/' + dst.lstrip('/')
    with open(src, 'rb') as fh:
        data = fh.read()
    fs.writebytes(dstpath, data)
    print(f"wrote {src} -> {dstpath} ({len(data)} bytes)")

fs.close()
