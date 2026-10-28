"""Check Juniper schema-1 scheduling invariants, not executable semantics."""

import argparse
import collections
import json
import sys


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify(module):
    require(module.get("schema") == 1, "unsupported machine IR schema")
    require(module.get("scheduled") is True, "input has not been scheduled")
    width = module["issue_width"]
    capacities = module["resources"]
    opcodes = set()
    count = 0
    for function in module["functions"]:
        instructions = function["instructions"]
        by_id = {inst["id"]: inst for inst in instructions}
        require(set(by_id) == set(range(len(instructions))),
                f"{function['name']}: invalid or duplicate instruction IDs")
        issued = collections.Counter()
        resources = collections.Counter()
        previous_cycle = -1
        for inst in instructions:
            cycle = inst["cycle"]
            require(isinstance(cycle, int) and cycle >= 0, "missing/invalid cycle")
            require(cycle >= previous_cycle, "instruction array is not in issue order")
            previous_cycle = cycle
            require(1 <= inst["latency"] <= 1000000, "invalid latency")
            resource = inst["resource"]
            require(resource in capacities, f"unknown resource {resource}")
            issued[cycle] += 1
            resources[cycle, resource] += 1
            require(issued[cycle] <= width, "issue width exceeded")
            require(resources[cycle, resource] <= capacities[resource],
                    "resource capacity exceeded")
            for dependency in inst["dependencies"]:
                require(dependency in by_id, f"unknown dependency {dependency}")
                producer = by_id[dependency]
                require(isinstance(producer["cycle"], int), "unscheduled producer")
                require(cycle >= producer["cycle"] + producer["latency"],
                        "producer latency violated")
            opcodes.add(inst["opcode"])
            count += 1
    return count, opcodes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input")
    parser.add_argument("--expect-opcode", action="append", default=[])
    parser.add_argument("--forbid-opcode", action="append", default=[])
    arguments = parser.parse_args()
    with open(arguments.input, encoding="utf-8") as stream:
        module = json.load(stream)
    count, opcodes = verify(module)
    for opcode in arguments.expect_opcode:
        require(opcode in opcodes, f"expected opcode {opcode}")
    for opcode in arguments.forbid_opcode:
        require(opcode not in opcodes, f"forbidden opcode {opcode}")
    print(f"validated {count} instructions across {len(module['functions'])} functions")


if __name__ == "__main__":
    try:
        main()
    except (KeyError, TypeError, ValueError, OSError) as error:
        print(f"schedule validation failed: {error}", file=sys.stderr)
        raise SystemExit(1)
