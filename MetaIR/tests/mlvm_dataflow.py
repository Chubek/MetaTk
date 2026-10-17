"""Independent small-CFG oracles and seeded differential optimizer tests."""
import json
from pathlib import Path
import random
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
CLI = str(Path(sys.argv[1]).resolve())
PREFIX = [CLI, "-L", str(ROOT / "MLVM/source")]


def successors(code, pc):
    op, _, _, _, target = code[pc]
    if op == 16:
        return []
    if op == 14:
        return [target]
    return list(dict.fromkeys([target, pc + 1])) if op == 15 else [pc + 1]


def reference(code):
    n = len(code)
    preds = [[] for _ in code]
    for pc in range(n):
        for target in successors(code, pc):
            preds[target].append(pc)
    visited, post = set(), []

    def visit(pc):
        if pc in visited:
            return
        visited.add(pc)
        for target in successors(code, pc):
            visit(target)
        post.append(pc)

    visit(0)
    # Deliberately use whole dominator sets, not MLVM's immediate-dominator
    # algorithm. These tiny graphs do not need production-sized storage.
    dom = {pc: ({0} if pc == 0 else set(visited)) for pc in visited}
    changed = True
    while changed:
        changed = False
        for pc in sorted(visited - {0}):
            incoming = [dom[p] for p in preds[pc] if p in visited]
            value = {pc} | set.intersection(*incoming)
            if dom[pc] != value:
                dom[pc] = value
                changed = True
    idom = [-1] * n
    idom[0] = 0
    for pc in visited - {0}:
        idom[pc] = max(dom[pc] - {pc}, key=lambda node: len(dom[node]))

    incoming, outgoing = [set() for _ in code], [set() for _ in code]
    changed = True
    while changed:
        changed = False
        for pc in sorted(visited, reverse=True):
            op, dst, a, b, _ = code[pc]
            uses = ({a} if 1 <= op <= 13 or op in (15, 16) else set())
            if 2 <= op <= 13:
                uses.add(b)
            defs = {dst} if op <= 13 else set()
            live = set().union(*(incoming[t] for t in successors(code, pc)))
            value = uses | (live - defs)
            if value != incoming[pc] or live != outgoing[pc]:
                incoming[pc], outgoing[pc] = value, live
                changed = True
    leaders = {0}
    for pc, row in enumerate(code):
        if row[0] in (14, 15):
            leaders.add(row[4])
        if row[0] >= 14 and pc + 1 < n:
            leaders.add(pc + 1)
    starts = sorted(leaders)
    blocks = [[a, b] for a, b in zip(starts, starts[1:] + [n])]
    mask = lambda regs: sum(1 << r for r in regs)
    return {"instruction_count": n, "reachable_count": len(visited), "block_count": len(blocks),
            "edge_count": sum(map(len, preds)), "reachable": [int(pc in visited) for pc in range(n)],
            "predecessors": preds, "reverse_postorder": post[::-1], "blocks": blocks,
            "immediate_dominators": idom,
            "liveness": [[mask(incoming[pc]), mask(outgoing[pc])] for pc in range(n)]}


def run(code, budget, states=None):
    regs, pc = [None] * 32, 0
    for _ in range(budget):
        if states is not None:
            state = states[pc]
            assert state[0] == 1
            for reg in range(32):
                if state[1] & (1 << reg):
                    assert regs[reg] == state[reg + 2], (pc, reg, regs, state)
                else:
                    assert state[reg + 2] == 0
        op, dst, a, b, imm = code[pc]
        if op == 16:
            return ("return", regs[a])
        if op == 14 or op == 15 and regs[a] == 0:
            pc = imm
            continue
        if op == 0:
            value = imm
        elif op == 2:
            value = regs[a] + regs[b]
        elif op == 3:
            value = regs[a] - regs[b]
        elif op == 4:
            value = regs[a] * regs[b]
        elif op == 9:
            value = regs[a] ^ regs[b]
        elif op == 15:
            pc += 1
            continue
        else:
            raise AssertionError(f"unsupported test opcode {op}")
        regs[dst] = (value + (1 << 63)) % (1 << 64) - (1 << 63)
        pc += 1
    return ("budget",)


def interference(code, report):
    graph = [0] * 32
    active = set()
    for pc, row in enumerate(code):
        if not report["reachable"][pc]:
            continue
        op, dst, a, b, _ = row
        if op <= 13:
            active.add(dst)
        if 1 <= op <= 13 or op in (15, 16):
            active.add(a)
        if 2 <= op <= 13:
            active.add(b)
        # Whole live sets form cliques; definition-vs-live-out additionally
        # prevents a dead destination from clobbering a live neighbor.
        masks = [report["liveness"][pc][0], report["liveness"][pc][1] | ((1 << dst) if op <= 13 else 0)]
        for mask in masks:
            members = {reg for reg in range(32) if mask & (1 << reg)}
            for reg in members:
                graph[reg] |= sum(1 << other for other in members - {reg})
    return graph, active


rng = random.Random(0x4D4C564D)
programs = []
for _ in range(24):
    n = rng.randrange(9, 23)
    code = [[0, reg, 0, 0, rng.randrange(-3, 4)] for reg in range(4)]
    for pc in range(4, n - 1):
        choice = rng.randrange(10)
        if choice < 6:
            code.append([rng.choice([2, 3, 4, 9]), rng.randrange(4), rng.randrange(4), rng.randrange(4), 0])
        elif choice < 8:
            code.append([14, 0, 0, 0, rng.randrange(n)])
        else:
            code.append([15, 0, rng.randrange(4), 0, rng.randrange(n)])
    code.append([16, 0, rng.randrange(4), 0, 0])
    programs.append(code)

# An irreducible loop with two entries, and unreachable predecessors.
initial = [[0, reg, 0, 0, reg + 1] for reg in range(4)]
programs.append(initial + [[15, 0, 0, 0, 7], [2, 1, 1, 0, 0], [14, 0, 0, 0, 8],
                           [3, 1, 1, 0, 0], [15, 0, 1, 0, 5], [15, 0, 0, 0, 7], [16, 0, 1, 0, 0]])
programs.append(initial + [[14, 0, 0, 0, 8], [0, 1, 0, 0, 99],
                           [14, 0, 0, 0, 8], [14, 0, 0, 0, 7], [16, 0, 1, 0, 0]])

for jit in (False, True):
    for start in range(0, len(programs), 4):
        batch = programs[start:start + 4]
        source = "use dataflow, optimization, register;\n"
        for code in batch:
            source += "{ let code = " + json.dumps(code) + "; print dataflow::report(code); "
            source += "print optimization::propagate(code); print optimization::constants(code); "
            source += "print register::interference(code); print register::color(code, 32); }\n"
        proc = subprocess.run(PREFIX + (["--jit"] if jit else []) + ["run", "-"], input=source,
                              capture_output=True, text=True, timeout=120)
        assert proc.returncode == 0, proc.stdout + proc.stderr
        lines = proc.stdout.splitlines()
        assert lines[-1] == "result 0" and len(lines) == 5 * len(batch) + 1
        for i, code in enumerate(batch):
            actual, optimized, states, graph, colors = map(json.loads, lines[5 * i:5 * i + 5])
            assert actual == reference(code), (code, actual, reference(code))
            expected_graph, active = interference(code, actual)
            assert graph == expected_graph
            assert len(colors) == 32
            for reg in range(32):
                assert 0 <= colors[reg] < 32 if reg in active else colors[reg] == -1
                for other in range(32):
                    if graph[reg] & (1 << other):
                        assert colors[reg] != colors[other]
            allocated = []
            for op, dst, a, b, imm in code:
                allocated.append([op, colors[dst] if op <= 13 and colors[dst] >= 0 else dst,
                                  colors[a] if (1 <= op <= 13 or op in (15, 16)) and colors[a] >= 0 else a,
                                  colors[b] if 2 <= op <= 13 and colors[b] >= 0 else b, imm])
            assert len(optimized) == len(code) and len(states) == len(code)
            for before, after in zip(code, optimized):
                assert before == after or (before[0] <= 13 and after[0] == 0 and before[1] == after[1])
            for pc, state in enumerate(states):
                assert len(state) == 34
                assert state[0] == actual["reachable"][pc]
                if not state[0]:
                    assert state == [0] * 34
            for budget in (1, len(code), 150):
                assert run(code, budget, states) == run(optimized, budget)
                assert run(code, budget) == run(allocated, budget)

print(f"MLVM dataflow: {len(programs)} independent CFG/differential cases passed on VM and JIT")
