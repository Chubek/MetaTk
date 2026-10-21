// aarch64.S - AAPCS64 call trampoline.
// Prototype (AAPCS64 entry):
//   x0=func, x1=gpr, x2=fpr, x3=stack, x4=stack_bytes, x5=hidden(x8), x6=out_gpr, x7=out_fpr
// gpr[8]=x0-x7, fpr[8]=v0-v7 low 64, hidden->x8, out_gpr[2]=x0,x1, out_fpr[4]=v0-v3.
.text
.arch armv8-a
.globl dinv_trampoline_aapcs64
.type dinv_trampoline_aapcs64,@function
dinv_trampoline_aapcs64:
  stp x29, x30, [sp, #-16]!
  mov x29, sp
  stp x19, x20, [sp, #-16]!
  stp x21, x22, [sp, #-16]!
  stp x23, x24, [sp, #-16]!
  stp x25, x26, [sp, #-16]!
  stp x27, x28, [sp, #-16]!
  // Save entry args to callee-saved.
  mov x19, x0   // func
  mov x20, x1   // gpr
  mov x21, x2   // fpr
  mov x22, x3   // stack data
  mov x23, x4   // stack bytes
  mov x24, x5   // hidden
  mov x25, x6   // out_gpr
  mov x26, x7   // out_fpr
  // Allocate N = round_up(stack_bytes, 16), keep sp 16-aligned (it is on entry).
  add x27, x23, #15
  and x27, x27, #~15
  sub sp, sp, x27
  cbz x23, .La_no_copy
  mov x0, sp
  mov x1, x22
  mov x2, x23
.La_copy:
  ldrb w28, [x1], #1
  strb w28, [x0], #1
  subs x2, x2, #1
  b.ne .La_copy
.La_no_copy:
  // Load GPRs x0-x7.
  ldp x0, x1, [x20, #0]
  ldp x2, x3, [x20, #16]
  ldp x4, x5, [x20, #32]
  ldp x6, x7, [x20, #48]
  // hidden -> x8
  mov x8, x24
  // Load FPRs d0-d7 (low 64 of v regs).
  ldp d0, d1, [x21, #0]
  ldp d2, d3, [x21, #16]
  ldp d4, d5, [x21, #32]
  ldp d6, d7, [x21, #48]
  blr x19
  // Save int returns.
  stp x0, x1, [x25, #0]
  // Save fp returns v0-v3 low 64.
  stp d0, d1, [x26, #0]
  stp d2, d3, [x26, #16]
  add sp, sp, x27
  ldp x27, x28, [sp], #16
  ldp x25, x26, [sp], #16
  ldp x23, x24, [sp], #16
  ldp x21, x22, [sp], #16
  ldp x19, x20, [sp], #16
  ldp x29, x30, [sp], #16
  ret
.size dinv_trampoline_aapcs64, .-dinv_trampoline_aapcs64
