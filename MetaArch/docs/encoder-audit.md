# Phase A: inventory and independent checks

Inventory taken before spec changes (original line numbers). All 51 sites are
listed below. No backend files were modified. TableGen references are the local
LLVM corpus under `/mnt/warble/domweave/devtools/archspec/LLVM-Target/`.
Assembler checks used LLVM 22.1.8 `llvm-mc -show-encoding`.

## Corrected (12 sites)

| Spec | Encoder (original line) | Reference |
|---|---|---|
| ppc32 | mulli (294), li (310), addi (314), addis (318) | PowerPC/PPCInstrInfo.td: MULLI, LI, ADDI, ADDIS; PPCInstrFormats.td: DForm_base, DForm_2_r0 |
| ppc64 | mulli (294), li (310), addi (314), addis (318) | Same definitions; little-endian serialization |
| mips32 | addi (281), addiu (285) | Mips/MipsInstrInfo.td: ADDi, ADDiu (`simm16_relaxed`, ADDI_FM) |
| mips64 | addi (281), addiu (285) | Same definitions; these are 32-bit arithmetic instructions, not DADDI/DADDIU |

Each corrected encoder has five independent assembler cases: -32768, -1, 0, 1,
32767. PowerPC triples are `ppc32` and `ppc64le`; MIPS triples are `mipsel`
and `mips64el`, matching each spec's byte order. The RISC-V control uses `riscv64`.
Example reproduction:

```
echo 'addi 3,3,-1' | llvm-mc -triple=ppc64le -show-encoding
echo 'addiu $4,$4,-32768' | llvm-mc -triple=mips64el -show-encoding
echo 'addi a0,a0,-1' | llvm-mc -triple=riscv64 -show-encoding
```

PowerPC numeric instruction 0x3863FFFF is `ff ff 63 38` in ppc64's LE target,
and `38 63 ff ff` in ppc32's BE target. TSV expectations are memory-order bytes.

## Unresolved, deliberately unchanged (39 sites)

These need more than a modulo substitution, or an explicit operand-contract
decision. The lint reports them; no `# biased` exemptions conceal them.

### ARC (25)

| Encoder | Original line | Field |
|---|---:|---|
| add_rs12 | 1096 | S12 |
| sub_rs12 | 1100 | S12 |
| mov_rs12 | 1105 | S12 |
| lr_rs12 | 1110 | S12 |
| sr_rs12 | 1118 | S12 |
| ld_rs9 | 1178 | S9 |
| ld_di_rs9 | 1192 | S9 |
| ld_aw_rs9 | 1196 | S9 |
| ldb_rs9 | 1200 | S9 |
| ldb_di_rs9 | 1214 | S9 |
| ldb_aw_rs9 | 1218 | S9 |
| ldh_rs9 | 1222 | S9 |
| ldh_di_rs9 | 1236 | S9 |
| ldh_aw_rs9 | 1240 | S9 |
| st_rs9 | 1244 | S9 |
| st_di_rs9 | 1258 | S9 |
| st_aw_rs9 | 1262 | S9 |
| stb_rs9 | 1266 | S9 |
| stb_di_rs9 | 1280 | S9 |
| stb_aw_rs9 | 1284 | S9 |
| sth_rs9 | 1288 | S9 |
| sth_di_rs9 | 1302 | S9 |
| sth_aw_rs9 | 1306 | S9 |
| ldbx_rs9 | 1310 | S9 |
| ldhx_rs9 | 1314 | S9 |

ARCInstrFormats.td `immS`, `F32_DOP_RS12`, `F32_SOP_RS12`,
`F32_LD_RS9`, `F32_ST_RS9` confirm signed fields, but different placements:
S12 is split into bits 11:6 and 5:0; S9 into 23:16 and 15. The specs multiply
contiguous fields by 4096 or 64 instead. Register placement and RS12 tied
operands also need review. No ARC assembler is installed. Updating only the
bias would not provide independently correct whole-instruction regressions.

### Other unresolved sites

| Spec | Encoders (original lines) | Evidence / decision needed |
|---|---|---|
| hexagon | r_addi (193), mpyi (197) | HexagonDepInstrInfo.td A2_addi uses a signed 16-bit split immediate, not the spec's 12-bit layout. M2_mpysmi is pseudo; M2_mpysin/M2_mpysip select negative/positive magnitude opcodes. `mpyi` accepts -255..255 without extension, rejecting -256 and 256. Its bias is not a plain two's-complement case. Opcodes/register positions also disagree with llvm-mc. |
| loongarch32 | lu12iw (295) | llvm-mc: `lu12i.w $a0,1` = `24 00 00 14`; spec has unshifted immediate and rd*32 instead of immediate*32 and rd. Need full field correction. |
| loongarch64 | lu12iw (295) | Same issue. |
| mips32 | lui (289) | MipsInstrInfo.td LUi takes uimm16_relaxed. llvm-mc accepts both -1 and 65535. Decide signed aliases versus strictly unsigned operand API/range; current forms declare signed -32768..32767. |
| mips64 | lui (289) | Same operand-contract conflict. |
| riscv32 | lui (341), auipc (345) | llvm-mc accepts unsigned 0..1048575, rejects -1. Spec declares signed -524288..524287 and omits *4096 for bits 31:12. Need unsigned contract plus field placement and range correction, not just wrap. |
| riscv64 | lui (341), auipc (345) | Same issue. |
| s390x | lghi (224), aghi (228), chi (232) | llvm-mc `lghi %r1,-1` = `a7 19 ff ff`. Spec uses r1*4096, contaminating the immediate, instead of r1*1048576. Modulo-only correction would still be wrong. |
| tricore | addi (131) | No TriCore TableGen target or independent assembler available. Immediate overlaps rd*4096 in current expression; cannot establish full encoding confidently. |

The brief permits unresolved sites. Correcting these additional field layouts
requires reviewed ISA-specific work, not a blanket replacement or invented bytes.
