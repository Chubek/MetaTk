// amd64_sysv.S - SysV AMD64 call trampoline for dyninvoke.
// Prototype:
//   void dinv_trampoline_sysv_amd64(void *func, const uint64_t *gpr,
//     const uint64_t *fpr, const uint8_t *stack, size_t stack_bytes,
//     const void *hidden_ignored, uint64_t *out_gpr, uint64_t *out_fpr);
//
// out_gpr[2] receives rax,rdx; out_fpr[4] receives xmm0,xmm1 (low 64 bits).
// gpr/fpr arrays are always 6/8 entries (zero-padded by the caller).
.text
.globl dinv_trampoline_sysv_amd64
.type dinv_trampoline_sysv_amd64,@function
dinv_trampoline_sysv_amd64:
  pushq %rbp
  movq %rsp, %rbp
  pushq %rbx
  pushq %r12
  pushq %r13
  pushq %r14
  pushq %r15
  subq $8, %rsp              // align: entry rsp%16==8, 6 pushes (48) -> still 8, minus 8 -> 0
  // Save pointers that live in arg regs which we will overwrite.
  movq %rdi, %r15            // func
  movq %rsi, %rbx            // gpr
  movq %rdx, %r12            // fpr
  movq 16(%rbp), %r13        // out_gpr (7th arg)
  movq 24(%rbp), %r14        // out_fpr (8th arg)
  // rcx = stack_data, r8 = stack_bytes still live.
  // Allocate stack-arg area: N = round_up(stack_bytes, 16).
  movq %r8, %rax
  addq $15, %rax
  andq $~15, %rax
  subq %rax, %rsp
  movq %rax, %r11            // N for later restore
  testq %r8, %r8
  jz .Lno_copy
  // rep movsb: rsi=src(rcx), rdi=dst(rsp), rcx=count(r8)
  movq %rcx, %rsi
  movq %rsp, %rdi
  movq %r8, %rcx
  rep movsb
.Lno_copy:
  // Load GPRs for the target.
  movq 0(%rbx), %rdi
  movq 8(%rbx), %rsi
  movq 16(%rbx), %rdx
  movq 24(%rbx), %rcx
  movq 32(%rbx), %r8
  movq 40(%rbx), %r9
  // Load FPRs.
  movq 0(%r12), %rax
  movq %rax, %xmm0
  movq 8(%r12), %rax
  movq %rax, %xmm1
  movq 16(%r12), %rax
  movq %rax, %xmm2
  movq 24(%r12), %rax
  movq %rax, %xmm3
  movq 32(%r12), %rax
  movq %rax, %xmm4
  movq 40(%r12), %rax
  movq %rax, %xmm5
  movq 48(%r12), %rax
  movq %rax, %xmm6
  movq 56(%r12), %rax
  movq %rax, %xmm7
  movq %r11, %rbx            // N to callee-saved (r11 is clobbered by call)
  callq *%r15
  // Save integer returns.
  movq %rax, 0(%r13)
  movq %rdx, 8(%r13)
  // Save FP returns (low 64 bits).
  movq %xmm0, %rax
  movq %rax, 0(%r14)
  movq %xmm1, %rax
  movq %rax, 8(%r14)
  movq %xmm2, %rax
  movq %rax, 16(%r14)
  movq %xmm3, %rax
  movq %rax, 24(%r14)
  // Restore stack area (N was moved to callee-saved rbx before call).
  addq %rbx, %rsp
  addq $8, %rsp
  popq %r15
  popq %r14
  popq %r13
  popq %r12
  popq %rbx
  popq %rbp
  ret
.size dinv_trampoline_sysv_amd64, .-dinv_trampoline_sysv_amd64
