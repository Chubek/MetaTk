# 28. Machine IR and its JSON inspection format

Selection returns a `MachineModule` in C++ and inspection JSON in the CLI. This
is still virtual-register IR. Instruction names and operands are explicit, but
there is no hidden physical allocation, ABI, relocation resolution, or executable
image behind the JSON.

## Inspect an actual selected program

<!-- check: juniper mode=select opcodes=MUL,RET -->
```juniper
fn multiply(x, y) {
  return mul(x, y);
}
```

Save it as `$WORK/multiply.junir`, then:

```sh
"$J" select "$WORK/multiply.junir" --no-preprocess -o "$WORK/multiply.json"
python3 -m json.tool "$WORK/multiply.json"
```

A selected multiplication instruction has this shape (an illustrative excerpt,
not a complete machine module):

```json
{
  "id": 0,
  "statement": 0,
  "opcode": "MUL",
  "result": "$v0",
  "operands": [
    {"kind": "reg", "value": "x"},
    {"kind": "reg", "value": "y"}
  ],
  "latency": 3,
  "resource": "alu",
  "barrier": false,
  "dependencies": [],
  "cycle": null
}
```

Top-level fields include `schema` (currently 1), module/backend names,
`scheduled`, `issue_width`, `resources`, and `functions`. Functions contain their
parameter names and instruction arrays. The backend name does not serialize the
complete source rule set; preserve canonical IR separately.

## Read operands and identities correctly

* `reg`: a virtual value name, either a source local/parameter or generated `$vN`.
* `imm`: a signed-64 integer operand, represented as a JSON number.
* `label`: a source or generated control-flow target.
* `symbol`: a callee name in structural call lowering.

An empty `result` means the instruction defines no register. Statement indices
are zero-based source-IR statement indices after any requested optimization;
they are not original macro-source line numbers. Instruction IDs are original
selection identities and remain stable after scheduling. They need not equal the
instruction's position in a reordered array.

## A small JSON consumer

<!-- check: python -->
```python
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    module = json.load(stream)
if module.get("schema") != 1:
    raise SystemExit("unsupported machine IR schema")
for function in module["functions"]:
    print("function", function["name"])
    for instruction in function["instructions"]:
        print(instruction["id"], instruction["opcode"],
              instruction["result"], instruction["cycle"])
```

Python preserves these integer values exactly. Consumers using binary64 JSON
numbers must avoid rounding signed-64 immediates; use a parser with integer
support. Do not compare large immediates through floating-point conversions.

## C++ access and format boundaries

`Instruction` directly owns operands, dependency IDs, and an optional cycle.
`emit_machine_json(machine)` serializes the supplied value; it is not a separate
validation pass. Use `select` and `schedule` to obtain checked stage outputs.
There is no machine-JSON importer: passing this JSON to `parse` is not a supported
round trip. Chapter 17 describes the actual round-trippable source format.

[Previous](27-fusion.md) · [Index](README.md) · [Next](29-scheduling.md)
