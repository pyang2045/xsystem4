#!/usr/bin/env python3
"""Name the bindings in an XSYS4_LINK_REPORT file.

usage: link-report.py <xsystem4 binary> <report>
Prints "library.function arguments symbol", sorted. Diff two of these to see
which HLL bindings a change moved for that game.
"""
import subprocess
import sys

binary, report = sys.argv[1:3]
symbols = {}
for line in subprocess.run(["nm", binary], capture_output=True, text=True, check=True).stdout.splitlines():
    fields = line.split()
    if len(fields) == 3 and fields[1] in "tT":
        symbols[int(fields[0], 16)] = fields[2].removeprefix("_")
base = next(a for a, n in symbols.items() if n == "init_libraries")

out = []
for line in open(report):
    name, args, offset = line.split()
    out.append(f"{name} {args} " + ("-" if offset == "-" else symbols.get(base + int(offset), "external")))
print("\n".join(sorted(out)))
