#!/usr/bin/env python3
import socket
import json
import sys
import re

QMP_SOCK = "/tmp/qmp.sock"

if len(sys.argv) != 2:
    print("usage: pagewalk.py <vaddr>")
    sys.exit(1)

VA = int(sys.argv[1], 16)

class QMP:
    def __init__(self, path):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect(path)
        self.f = self.sock.makefile("rwb", buffering=0)

        self._recv()  # greeting
        self.cmd({"execute": "qmp_capabilities"})

    def _recv(self):
        return json.loads(self.f.readline().decode())

    def cmd(self, obj):
        self.f.write((json.dumps(obj) + "\n").encode())
        return self._recv()

    def hmp(self, line):
        r = self.cmd({
            "execute": "human-monitor-command",
            "arguments": {"command-line": line}
        })

        if "error" in r:
            raise RuntimeError(f"HMP error: {r['error']}")

        return r.get("return", "")

qmp = QMP(QMP_SOCK)

def abort(msg):
    print(msg)
    sys.exit(1)

def read_qword(pa):
    out = qmp.hmp(f"xp /1gx {pa:#x}")
    m = re.search(r"0x[0-9a-fA-F]+", out)
    if not m:
        raise RuntimeError(f"memory read failed at {pa:#x}")
    return int(m.group(0), 16)

def flags(e):
    f = []
    if e & (1 << 0):  f.append("P")
    if e & (1 << 1):  f.append("RW")
    if e & (1 << 2):  f.append("US")
    if e & (1 << 7):  f.append("PS")
    if e & (1 << 63): f.append("NX")
    return "|".join(f) if f else "-"

regs = qmp.hmp("info registers")
m = re.search(r"CR3=([0-9a-fA-Fx]+)", regs)
if not m:
    abort("Cannot read CR3")

CR3 = int(m.group(1), 16)
PML4_BASE = CR3 & 0x000ffffffffff000

pml4_i = (VA >> 39) & 0x1ff
pdpt_i = (VA >> 30) & 0x1ff
pd_i   = (VA >> 21) & 0x1ff
pt_i   = (VA >> 12) & 0x1ff
offset = VA & 0xfff

print(f"Virtual Address: 0x{VA:x}")
print(f"PML4 base:       0x{PML4_BASE:x}")

PML4E = read_qword(PML4_BASE + pml4_i * 8)
PDPT_BASE = PML4E & 0x000ffffffffff000

print(f"PML4E [{pml4_i}] => {flags(PML4E)} | 0x{PDPT_BASE:x}")

if not (PML4E & 1):
    abort("PML4E not present")

PDPTE = read_qword(PDPT_BASE + pdpt_i * 8)

print(f"PDPTE [{pdpt_i}] => {flags(PDPTE)}")

if not (PDPTE & 1):
    abort("PDPTE not present")

if PDPTE & (1 << 7):
    page_base = PDPTE & 0x000fffffc0000000
    offset = VA & 0x3fffffff
    PA = page_base | offset

    print("-> 1 GiB page")
    print(f"Page base:        0x{page_base:x}")
    print(f"Offset:           0x{offset:x}")
    print(f"Physical Address: 0x{PA:x}")
    sys.exit(0)

PD_BASE = PDPTE & 0x000ffffffffff000

PDE = read_qword(PD_BASE + pd_i * 8)

print(f"PDE   [{pd_i}] => {flags(PDE)}")

if not (PDE & 1):
    abort("PDE not present")

if PDE & (1 << 7):
    page_base = PDE & 0x000fffffffe00000
    offset = VA & 0x1fffff
    PA = page_base | offset

    print("-> 2 MiB page")
    print(f"Page base:        0x{page_base:x}")
    print(f"Offset:           0x{offset:x}")
    print(f"Physical Address: 0x{PA:x}")
    sys.exit(0)

PT_BASE = PDE & 0x000ffffffffff000

print(f"PT_BASE = {PT_BASE:#x}")
print(f"pt_i = {pt_i:#x}")
print(f"access = {PT_BASE + pt_i * 8:#x}")

PTE = read_qword(PT_BASE + pt_i * 8)
PAGE_BASE = PTE & 0x000ffffffffff000

print(f"PTE   [{pt_i}] => {flags(PTE)} | 0x{PAGE_BASE:x}")

if not (PTE & 1):
    abort("PTE not present")

PA = PAGE_BASE | offset

print("-> 4 KiB page")
print(f"Page base:        0x{PAGE_BASE:x}")
print(f"Offset:           0x{offset:x}")
print(f"Physical Address: 0x{PA:x}")
