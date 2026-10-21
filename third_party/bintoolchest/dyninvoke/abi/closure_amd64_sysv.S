// closure_amd64_sysv.S - closure dispatcher for SysV AMD64.
// r10 = closure* (set by the generated stub), other regs hold the target args.
// Calls: void dinv_closure_invoke_sysv(void*, const uint64_t*, const uint64_t*,
//                                      const uint8_t*, uint64_t*, uint64_t*);
.text
.globl dinv_closure_dispatcher_sysv
.type dinv_closure_dispatcher_sysv,@function
dinv_closure_dispatcher_sysv:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rbx
  pushq %r12
  pushq %r13
  pushq %r14
  pushq %r15
  subq $8, %rsp
  // 160 bytes: gpr[6] + fpr[8] + out_gpr[2] + out_fpr[4]
  subq $160, %rsp
  // Save incoming GPRs.
  movq %rdi, 0(%rsp)
  movq %rsi, 8(%rsp)
  movq %rdx, 16(%rsp)
  movq %rcx, 24(%rsp)
  movq %r8, 32(%rsp)
  movq %r9, 40(%rsp)
  // Save incoming FPRs.
  movq %xmm0, %rax
  movq %rax, 48(%rsp)
  movq %xmm1, %rax
  movq %rax, 56(%rsp)
  movq %xmm2, %rax
  movq %rax, 64(%rsp)
  movq %xmm3, %rax
  movq %rax, 72(%rsp)
  movq %xmm4, %rax
  movq %rax, 80(%rsp)
  movq %xmm5, %rax
  movq %rax, 88(%rsp)
  movq %xmm6, %rax
  movq %rax, 96(%rsp)
  movq %xmm7, %rax
  movq %rax, 104(%rsp)
  // Args for the C helper.
  movq %r10, %rdi          // closure
  movq %rsp, %rsi          // gpr
  leaq 48(%rsp), %rdx      // fpr
  leaq 16(%rbp), %rcx      // original stack args
  leaq 112(%rsp), %r8      // out_gpr
  leaq 128(%rsp), %r9      // out_fpr
  callq dinv_closure_invoke_sysv
  // Load returns.
  movq 128(%rsp), %rax
  movq %rax, %xmm0
  movq 136(%rsp), %rax
  movq %rax, %xmm1
  movq 144(%rsp), %rax
  movq %rax, %xmm2
  movq 152(%rsp), %rax
  movq %rax, %xmm3
  movq 112(%rsp), %rax
  movq 120(%rsp), %rdx
  addq $160, %rsp
  addq $8, %rsp
  popq %r15
  popq %r14
  popq %r13
  popq %r12
  popq %rbx
  popq %rbp
  ret
.size dinv_closure_dispatcher_sysv, .-dinv_closure_dispatcher_sysv
