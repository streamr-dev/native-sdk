#!/usr/bin/env python3
"""Verify that each package's internal header graph is a DAG.

The planned C++ modules consolidation (MODERNIZATION.md Phase 2.6) maps
headers onto module partitions, and module imports must be acyclic — so
the header dependency graph has to be a DAG. Two edge kinds are checked:

  * include edges:   #include "streamr-<pkg>/..." between headers of the
                     same package
  * semantic edges:  a forward declaration (`class X;` / `struct X;`) of
                     a type DEFINED in another header of the package.
                     Forward declarations break cycles textually, but a
                     forward-declared entity owned by another partition
                     still forces a cyclic import after consolidation.

Run from the repository root: ./check-include-dag.py
Exits non-zero listing every cycle found.
"""

import os
import re
import sys
from collections import defaultdict

INC_RE = re.compile(r'#include\s+"((?:streamr|packages)[^"]*)"')
FWD_RE = re.compile(r"^\s*(?:class|struct)\s+([A-Za-z_]\w*)\s*;")
DEF_RE = re.compile(r"(?:class|struct)\s+([A-Za-z_]\w*)[^;{]*\{")


def check_package(include_root: str) -> list[list[str]]:
    nodes: set[str] = set()
    texts: dict[str, str] = {}
    for dirpath, _, files in os.walk(include_root):
        for f in files:
            if not f.endswith(".hpp"):
                continue
            rel = os.path.relpath(os.path.join(dirpath, f), include_root)
            nodes.add(rel)
            with open(
                os.path.join(dirpath, f), encoding="utf-8", errors="replace"
            ) as fh:
                texts[rel] = fh.read()

    owner: dict[str, str] = {}
    for rel, txt in texts.items():
        for m in DEF_RE.finditer(txt):
            owner.setdefault(m.group(1), rel)

    edges: dict[str, set[str]] = defaultdict(set)
    for rel, txt in texts.items():
        for line in txt.splitlines():
            m = INC_RE.search(line)
            if m and m.group(1) in nodes:
                edges[rel].add(m.group(1))
            fm = FWD_RE.match(line)
            if fm:
                own = owner.get(fm.group(1))
                if own and own != rel:
                    edges[rel].add(own)

    # Tarjan SCC
    sys.setrecursionlimit(10000)
    counter = [0]
    stack: list[str] = []
    index: dict[str, int] = {}
    lowlink: dict[str, int] = {}
    on_stack: dict[str, bool] = {}
    sccs: list[list[str]] = []

    def strongconnect(v: str) -> None:
        index[v] = lowlink[v] = counter[0]
        counter[0] += 1
        stack.append(v)
        on_stack[v] = True
        for w in edges.get(v, ()):
            if w not in index:
                strongconnect(w)
                lowlink[v] = min(lowlink[v], lowlink[w])
            elif on_stack.get(w):
                lowlink[v] = min(lowlink[v], index[w])
        if lowlink[v] == index[v]:
            scc = []
            while True:
                w = stack.pop()
                on_stack[w] = False
                scc.append(w)
                if w == v:
                    break
            sccs.append(scc)

    for v in sorted(nodes):
        if v not in index:
            strongconnect(v)

    return [s for s in sccs if len(s) > 1]


def main() -> int:
    packages_dir = "packages"
    if not os.path.isdir(packages_dir):
        print("check-include-dag.py: run from the repository root", file=sys.stderr)
        return 2

    failed = False
    for pkg in sorted(os.listdir(packages_dir)):
        include_root = os.path.join(packages_dir, pkg, "include")
        if not os.path.isdir(include_root):
            continue
        cycles = check_package(include_root)
        if cycles:
            failed = True
            print(f"{pkg}: header dependency cycles found:")
            for scc in sorted(cycles, key=len, reverse=True):
                print(f"  cycle of {len(scc)} headers:")
                for h in sorted(scc):
                    print(f"    {h}")
        else:
            print(f"{pkg}: OK")

    if failed:
        print(
            "\nHeader cycles block the module consolidation "
            "(see MODERNIZATION.md Phase 2.6). Break the cycle with an "
            "abstract interface or by merging/splitting headers.",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
