# Explicit encoder parameters avoid guessing architectural operand order.
.global entry
.text
entry:
    .insn addi rd=3, ra=3, imm=-1
    .insn addi rd=3, ra=3, imm=1
.data
    .word entry
    .asciz "MetaAS"
