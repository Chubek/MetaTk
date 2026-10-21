// win64.S - Windows x64 call trampoline (also runs on SysV hosts).
// Same prototype as SysV. Only gpr[0..3]/fpr[0..3] are used; out_gpr[0]=rax,
// out_fpr[0]=xmm0 low 64. Reserves the 32-byte shadow space.
.text
.globl dinv_trampoline_win64
.type dinv_trampoline_win64,@function
dinv_trampoline_win64:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rbx
  pushq %r12
  pushq %r13
  pushq %r14
  pushq %r15
  subq $8, %rsp
#ifdef _WIN32
  // Win64 entry: rcx=func, rdx=gpr, r8=fpr, r9=stack,
  // [rbp+16]=stack_bytes, [rbp+24]=hidden(ignored), [rbp+32]=out_gpr, [rbp+40]=out_fpr.
  movq %rcx, %r15
  movq %rdx, %rbx
  movq %r8, %r12
  movq %r9, %rcx            // stack_data
  movq 16(%rbp), %r8       // stack_bytes
  movq 32(%rbp), %r13
  movq 40(%rbp), %r14
#else
  movq %rdi, %r15            // func
  movq %rsi, %rbx            // gpr
  movq %rdx, %r12            // fpr
  movq 16(%rbp), %r13        // out_gpr
  movq 24(%rbp), %r14        // out_fpr
  // rcx = stack_data, r8 = stack_bytes still live.
#endif
  // N = 32 (shadow) + round_up(stack_bytes, 16)
  movq %r8, %rax
  addq $15, %rax
  andq $~15, %rax
  addq $32, %rax
  subq %rax, %rsp
  movq %rax, %r11
  testq %r8, %r8
  jz .Lw_no_copy
  leaq 32(%rsp), %rdi
  movq %rcx, %rsi
  movq %r8, %rcx
  rep movsb
.Lw_no_copy:
  movq 0(%rbx), %rcx
  movq 8(%rbx), %rdx
  movq 16(%rbx), %r8
  movq 24(%rbx), %r9
  movq 0(%r12), %rax
  movq %rax, %xmm0
  movq 8(%r12), %rax
  movq %rax, %xmm1
  movq 16(%r12), %rax
  movq %rax, %xmm2
  movq 24(%r12), %rax
  movq %rax, %xmm3
  movq %r11, %rbx
  callq *%r15
  movq %rax, 0(%r13)
  movq %xmm0, %rax
  movq %rax, 0(%r14)
  movq %xmm1, %rax
  movq %rax, 8(%r14)
  movq %xmm2, %rax
  movq %rax, 16(%r14)
  movq %xmm3, %rax
  movq %rax, 24(%r14)
  addq %rbx, %rsp
  addq $8, %rsp
  popq %r15
  popq %r14
  popq %r13
  popq %r12
  popq %rbx
  popq %rbp
  ret
.size dinv_trampoline_win64, .-dinv_trampoline_win64
