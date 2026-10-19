.equ VALUE, 6 * 7
.global entry
.text
entry:
    mov r1, VALUE
    copy r0, end-entry
    .align 4
end:
    ret
.data
    .quad entry
    .asciz "MetaAS"
