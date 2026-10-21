// riscv64.S - RISC-V LP64D call trampoline (revised).
// a0=func, a1=gpr, a2=fpr, a3=stack, a4=stack_bytes, a5=hidden(ignored),
// a6=out_gpr, a7=out_fpr.
.text
.globl dinv_trampoline_riscv64
.type dinv_trampoline_riscv64,@function
dinv_trampoline_riscv64:
  addi sp, sp, -80
  sd ra, 72(sp)
  sd s0, 64(sp)
  sd s1, 56(sp)
  sd s2, 48(sp)
  sd s3, 40(sp)
  sd s4, 32(sp)
  sd s5, 24(sp)
  sd s6, 16(sp)
  sd s7, 8(sp)
  sd s8, 0(sp)
  mv s1, a0
  mv s2, a1
  mv s3, a2
  mv s4, a3
  mv s5, a4
  mv s6, a6
  mv s7, a7
  addi t0, s5, 15
  andi t0, t0, -16
  // stash N in s8
  mv s8, t0
  sub sp, sp, t0
  beqz s5, .Lr_no_copy
  mv t1, sp
  mv t2, s4
  mv t3, s5
.Lr_copy:
  beqz t3, .Lr_no_copy
  lbu t4, 0(t2)
  sb t4, 0(t1)
  addi t1, t1, 1
  addi t2, t2, 1
  addi t3, t3, -1
  j .Lr_copy
.Lr_no_copy:
  ld a0, 0(s2)
  ld a1, 8(s2)
  ld a2, 16(s2)
  ld a3, 24(s2)
  ld a4, 32(s2)
  ld a5, 40(s2)
  ld a6, 48(s2)
  ld a7, 56(s2)
  fld fa0, 0(s3)
  fld fa1, 8(s3)
  fld fa2, 16(s3)
  fld fa3, 24(s3)
  fld fa4, 32(s3)
  fld fa5, 40(s3)
  fld fa6, 48(s3)
  fld fa7, 56(s3)
  jalr s1
  sd a0, 0(s6)
  sd a1, 8(s6)
  fsd fa0, 0(s7)
  fsd fa1, 8(s7)
  fsd fa2, 16(s7)
  fsd fa3, 24(s7)
  add sp, sp, s8
  ld ra, 72(sp)
  ld s0, 64(sp)
  ld s1, 56(sp)
  ld s2, 48(sp)
  ld s3, 40(sp)
  ld s4, 32(sp)
  ld s5, 24(sp)
  ld s6, 16(sp)
  ld s7, 8(sp)
  ld s8, 0(sp)
  addi sp, sp, 80
  ret
.size dinv_trampoline_riscv64, .-dinv_trampoline_riscv64
