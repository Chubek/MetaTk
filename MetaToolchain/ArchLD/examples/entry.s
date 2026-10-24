.global _start
.text
_start:
    call run
    mov %eax, %edi
    mov $60, %eax
    syscall
