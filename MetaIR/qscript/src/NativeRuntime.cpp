// The Linux x86-64 run-time for the QScript built-ins (not installed API).
//
// The native backend places this text after the program, and the program's
// host calls reach it through qs_rt_builtin. It ports CRuntime.cpp: the same
// heap accounting, the same messages and the same results. The text is Intel
// syntax for the ASMtk assembler, which the repository's toolchain assembles
// and LDtk links, so no external assembler, linker or C compiler is needed.
//
// Limits of the assembler shape the code: there is one .text section (so the
// heap, the descriptors and the scratch buffer live in mappings made at start-up),
// no scaled indexing, and no movzx, so bytes are loaded into zeroed registers.
//
// Failures print "qscript: MESSAGE" on standard error and exit at once: status
// 126 for a trap and 125 for a resource limit. The first failure therefore wins,
// as it does in the C run-time, and no output follows it.
#include "qscript/src/Runtime.h"
#include "qscript/src/Types.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace qscript::detail {
namespace {

// Routines. Tokens between @ signs are expanded by native_runtime_text().
// Calling convention of the helpers: arguments in rdi, rsi, rdx, rcx and r8, the
// result in rax. Helpers may clobber every register except rbp, rsp, r13, r14
// and r15; the routines that use rbx and r12 save them on the stack.
// The state block (r13) holds the run's heap and scratch fields at the @S_*@ offsets.
constexpr std::string_view routines = R"QSASM(
; ---- start-up ----
qs_rt_init:
  mov rdi, @STATE_SIZE@
  call qs_rt_map
  mov r13, rax
  mov rdi, @DESC_BYTES@
  call qs_rt_map
  mov [r13+@S_DESC@], rax
  mov rdi, @LIT_BYTES@
  call qs_rt_map
  mov [r13+@S_LIT@], rax
  ret

; rdi = bytes -> rax = zeroed private mapping of at least that size
qs_rt_map:
  add rdi, 4095
  and rdi, -4096
  mov rsi, rdi
  xor rdi, rdi
  mov rdx, 3
  mov r10, 16418
  mov r8, -1
  xor r9, r9
  mov rax, 9
  syscall
  mov rcx, -4096
  cmp rax, rcx
  jae qs_rt_map_fail
  ret
qs_rt_map_fail:
  @LIMIT:oom@

; ---- failures and output ----
; rdi = message, rsi = length, rdx = exit status
qs_rt_fail_common:
  push rdx
  push rdi
  push rsi
  xor rax, rax
  mov [r13+@S_SLEN@], rax
  @APP:prefix@
  mov rdi, [rsp+8]
  mov rsi, [rsp]
  call qs_rt_app
  @APP:newline@
  mov rdi, [rsp+16]
  add rsp, 24
  jmp qs_rt_flush
qs_rt_trap:
  mov rdx, 126
  jmp qs_rt_fail_common
qs_rt_limit:
  mov rdx, 125
  jmp qs_rt_fail_common

; rdi = status: writes the scratch text to standard error and exits
qs_rt_flush:
  push rdi
  mov rdi, 2
  mov rsi, [r13+@S_SPTR@]
  mov rdx, [r13+@S_SLEN@]
  call qs_rt_write
  pop rdi
  mov rax, 60
  syscall
  ud2

; rdi = descriptor, rsi = buffer, rdx = count -> rax = 0, or -1 on failure
qs_rt_write:
  push rbx
  push r12
  push rdi
  mov rbx, rsi
  mov r12, rdx
qs_rt_write_loop:
  cmp r12, 0
  je qs_rt_write_ok
  mov rdi, [rsp]
  mov rsi, rbx
  mov rdx, r12
  mov rax, 1
  syscall
  cmp rax, -4
  je qs_rt_write_loop
  cmp rax, 0
  jle qs_rt_write_fail
  add rbx, rax
  sub r12, rax
  jmp qs_rt_write_loop
qs_rt_write_ok:
  mov rax, 0
  add rsp, 8
  pop r12
  pop rbx
  ret
qs_rt_write_fail:
  mov rax, -1
  add rsp, 8
  pop r12
  pop rbx
  ret

; ---- memory ----
; rdi = bytes -> rax = 8-aligned block of that size from the arena (0 bytes: no space needed)
qs_rt_arena:
  add rdi, 7
  and rdi, -8
  mov rax, [r13+@S_CUR@]
  cmp rdi, 0
  je qs_rt_arena_done
  mov rdx, rax
  add rdx, rdi
  mov rcx, [r13+@S_END@]
  cmp rdx, rcx
  jbe qs_rt_arena_take
  mov rsi, @CHUNK@
  cmp rdi, rsi
  jbe qs_rt_arena_chunk
  mov rsi, rdi
qs_rt_arena_chunk:
  add rsi, 4095
  and rsi, -4096
  push rdi
  push rsi
  mov rdi, rsi
  call qs_rt_map
  pop rsi
  pop rdi
  mov [r13+@S_CUR@], rax
  add rax, rsi
  mov [r13+@S_END@], rax
  jmp qs_rt_arena
qs_rt_arena_take:
  mov [r13+@S_CUR@], rdx
  ret
qs_rt_arena_done:
  ret

; rdi = bytes: charges the heap budget as Session::charge_bytes does
qs_rt_charge:
  mov rax, rdi
  mov rcx, @MAX_BYTES@
  cmp rax, rcx
  ja qs_rt_charge_fail
  sub rcx, rax
  mov rdx, [r13+@S_USED@]
  cmp rdx, rcx
  ja qs_rt_charge_fail
  add rdx, rax
  mov [r13+@S_USED@], rdx
  ret
qs_rt_charge_fail:
  @LIMIT:heap_bytes@

; rdi = kind, rsi = payload bytes -> rax = new handle (1-based)
qs_rt_alloc:
  push rbx
  push r12
  mov rbx, rdi
  mov r12, rsi
  mov rax, [r13+@S_COUNT@]
  mov rcx, @MAX_OBJECTS@
  cmp rax, rcx
  jae qs_rt_alloc_objects
  lea rdi, [r12+64]
  call qs_rt_charge
  mov rax, [r13+@S_COUNT@]
  add rax, 1
  mov [r13+@S_COUNT@], rax
  mov rdx, rax
  sub rdx, 1
  shl rdx, 5
  mov rcx, [r13+@S_DESC@]
  add rdx, rcx
  mov [rdx], rbx
  xor rcx, rcx
  mov [rdx+8], rcx
  mov [rdx+16], rcx
  mov [rdx+24], rcx
  pop r12
  pop rbx
  ret
qs_rt_alloc_objects:
  @LIMIT:objects@

; rdi = handle -> rax = its descriptor (no checks)
qs_rt_slot:
  mov rax, rdi
  sub rax, 1
  shl rax, 5
  mov rcx, [r13+@S_DESC@]
  add rax, rcx
  ret

; rdi = handle, rsi = kind -> rax = descriptor, or the internal trap
qs_rt_desc:
  cmp rdi, 0
  jle qs_rt_desc_bad
  mov rax, [r13+@S_COUNT@]
  cmp rdi, rax
  ja qs_rt_desc_bad
  mov rax, rdi
  sub rax, 1
  shl rax, 5
  mov rcx, [r13+@S_DESC@]
  add rax, rcx
  mov rcx, [rax]
  cmp rcx, rsi
  jne qs_rt_desc_bad
  ret
qs_rt_desc_bad:
  @TRAP:invalid@

; rdi = type descriptor -> rax = its kind, or the internal trap
qs_rt_kind_of:
  cmp rdi, @NTYPES@
  jae qs_rt_kind_bad
  mov rax, rdi
  shl rax, 4
  lea rcx, [rel qs_types]
  add rax, rcx
  mov rax, [rax]
  ret
qs_rt_kind_bad:
  @TRAP:badtype@

; rdi = byte count -> rax = handle of a new string of that length (bytes not yet written)
qs_rt_str_new:
  push rbx
  push r12
  mov rbx, rdi
  mov rsi, rdi
  mov rdi, @KIND_STR@
  call qs_rt_alloc
  mov r12, rax
  mov rdi, rbx
  call qs_rt_arena
  mov rdx, r12
  sub rdx, 1
  shl rdx, 5
  mov rcx, [r13+@S_DESC@]
  add rdx, rcx
  mov [rdx+24], rax
  mov [rdx+8], rbx
  mov [rdx+16], rbx
  mov rax, r12
  pop r12
  pop rbx
  ret

; rdi = source, rsi = byte count -> rax = handle of a copy
qs_rt_str_copy:
  push rbx
  push r12
  mov rbx, rdi
  mov r12, rsi
  mov rdi, r12
  call qs_rt_str_new
  mov r8, rax
  mov rcx, rax
  sub rcx, 1
  shl rcx, 5
  mov rdx, [r13+@S_DESC@]
  add rcx, rdx
  mov rdi, [rcx+24]
  mov rsi, rbx
  mov rdx, r12
  call qs_rt_copy
  mov rax, r8
  pop r12
  pop rbx
  ret

; rdi = destination, rsi = source, rdx = count (clobbers rax, rdi, rsi, rdx)
qs_rt_copy:
  cmp rdx, 0
  je qs_rt_copy_done
qs_rt_copy_loop:
  mov al, [rsi]
  mov [rdi], al
  add rsi, 1
  add rdi, 1
  sub rdx, 1
  jne qs_rt_copy_loop
qs_rt_copy_done:
  ret

; rdi = list descriptor, rsi = needed capacity (in items)
qs_rt_reserve:
  push rbx
  push r12
  mov rbx, rdi
  mov r12, rsi
  mov rcx, [rbx+16]
  cmp r12, rcx
  jbe qs_rt_reserve_done
  cmp rcx, 0
  jne qs_rt_reserve_size
  mov rcx, 4
qs_rt_reserve_size:
  cmp rcx, r12
  jae qs_rt_reserve_alloc
  shl rcx, 1
  jmp qs_rt_reserve_size
qs_rt_reserve_alloc:
  push rcx
  mov rdi, rcx
  shl rdi, 3
  call qs_rt_arena
  pop rcx
  mov r8, [rbx+24]
  mov [rbx+24], rax
  mov [rbx+16], rcx
  mov rdi, rax
  mov rsi, r8
  mov rdx, [rbx+8]
  shl rdx, 3
  call qs_rt_copy
qs_rt_reserve_done:
  pop r12
  pop rbx
  ret

; rdi = index, rsi = length: traps unless 0 <= index < length
qs_rt_check_index:
  cmp rdi, rsi
  jae qs_rt_index_error
  ret
qs_rt_index_error:
  push rdi
  push rsi
  xor rax, rax
  mov [r13+@S_SLEN@], rax
  @APP:prefix@
  @APP:index@
  mov rdi, [rsp+8]
  call qs_rt_app_int
  @APP:outrange@
  mov rdi, [rsp]
  call qs_rt_app_int
  @APP:rparen_nl@
  add rsp, 16
  mov rdi, 126
  jmp qs_rt_flush

; ---- scratch text ----
; rdi = pointer, rsi = count: appends to the scratch buffer
qs_rt_app:
  push rbx
  push r12
  mov rbx, rdi
  mov r12, rsi
  mov rax, [r13+@S_SLEN@]
  add rax, r12
  mov rcx, [r13+@S_SCAP@]
  cmp rax, rcx
  jbe qs_rt_app_fit
  mov rdi, rax
  call qs_rt_grow
qs_rt_app_fit:
  mov rdi, [r13+@S_SPTR@]
  mov rax, [r13+@S_SLEN@]
  add rdi, rax
  mov rsi, rbx
  mov rdx, r12
  call qs_rt_copy
  mov rax, [r13+@S_SLEN@]
  add rax, r12
  mov [r13+@S_SLEN@], rax
  pop r12
  pop rbx
  ret

; rdi = needed length: moves the scratch buffer to a larger mapping
qs_rt_grow:
  push rbx
  push r12
  mov rbx, rdi
  mov r12, [r13+@S_SCAP@]
  cmp r12, 0
  jne qs_rt_grow_size
  mov r12, 64
qs_rt_grow_size:
  cmp r12, rbx
  jae qs_rt_grow_map
  shl r12, 1
  jmp qs_rt_grow_size
qs_rt_grow_map:
  mov rdi, r12
  call qs_rt_map
  mov [r13+@S_GTMP@], rax
  mov rdi, rax
  mov rsi, [r13+@S_SPTR@]
  mov rdx, [r13+@S_SLEN@]
  call qs_rt_copy
  mov rdi, [r13+@S_SPTR@]
  cmp rdi, 0
  je qs_rt_grow_done
  mov rsi, [r13+@S_SCAP@]
  mov rax, 11
  syscall
qs_rt_grow_done:
  mov rax, [r13+@S_GTMP@]
  mov [r13+@S_SPTR@], rax
  mov [r13+@S_SCAP@], r12
  pop r12
  pop rbx
  ret

; rdi = signed value: appends its decimal form
qs_rt_app_int:
  push rbx
  push r12
  mov rbx, rdi
  test rbx, rbx
  jns qs_rt_app_int_digits
  @APP:minus@
  mov rax, rbx
  neg rax
  mov rbx, rax
qs_rt_app_int_digits:
  mov rax, rbx
  sub rsp, 32
  mov r12, 32
qs_rt_app_int_loop:
  sub r12, 1
  xor rdx, rdx
  mov rcx, 10
  div rcx
  add rdx, 48
  mov rcx, rsp
  add rcx, r12
  mov [rcx], dl
  cmp rax, 0
  jne qs_rt_app_int_loop
  mov rdi, rsp
  add rdi, r12
  mov rsi, 32
  sub rsi, r12
  call qs_rt_app
  add rsp, 32
  pop r12
  pop rbx
  ret

; rdi = value, rsi = type descriptor, rdx = quote (1 for strings inside lists)
; appends the text of the value as Session::format_into does
qs_rt_fmt:
  push rbx
  push r12
  push rdx
  mov rbx, rdi
  mov r12, rsi
  cmp r12, @NTYPES@
  jae qs_rt_fmt_bad_type
  mov rax, r12
  shl rax, 4
  lea rcx, [rel qs_types]
  add rax, rcx
  mov rax, [rax]
  cmp rax, @KIND_INT@
  je qs_rt_fmt_int
  cmp rax, @KIND_STR@
  je qs_rt_fmt_str
  cmp rax, @KIND_LIST@
  je qs_rt_fmt_list
  @TRAP:badprint@
qs_rt_fmt_bad_type:
  @TRAP:badtype@
qs_rt_fmt_int:
  mov rdi, rbx
  call qs_rt_app_int
  jmp qs_rt_fmt_ret
qs_rt_fmt_str:
  mov rdi, [rsp]
  cmp rdi, 0
  je qs_rt_fmt_str_body
  @APP:quote@
qs_rt_fmt_str_body:
  mov rdi, rbx
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov rsi, [rax+8]
  mov rdi, [rax+24]
  call qs_rt_app
  mov rdi, [rsp]
  cmp rdi, 0
  je qs_rt_fmt_ret
  @APP:quote@
  jmp qs_rt_fmt_ret
qs_rt_fmt_list:
  @APP:lbracket@
  mov rdi, rbx
  mov rsi, @KIND_LIST@
  call qs_rt_desc
  mov rcx, [rax+8]
  mov rdx, [rax+24]
  sub rsp, 32
  mov [rsp+8], rcx
  mov [rsp+16], rdx
  mov rax, r12
  shl rax, 4
  lea rcx, [rel qs_types]
  add rax, rcx
  mov rax, [rax+8]
  mov [rsp+24], rax
  xor rax, rax
  mov [rsp], rax
qs_rt_fmt_list_loop:
  mov rax, [rsp]
  mov rcx, [rsp+8]
  cmp rax, rcx
  jae qs_rt_fmt_list_end
  cmp rax, 0
  je qs_rt_fmt_list_item
  @APP:comma@
qs_rt_fmt_list_item:
  mov rax, [rsp]
  shl rax, 3
  mov rcx, [rsp+16]
  add rcx, rax
  mov rdi, [rcx]
  mov rsi, [rsp+24]
  mov rdx, 1
  call qs_rt_fmt
  mov rax, [rsp]
  add rax, 1
  mov [rsp], rax
  jmp qs_rt_fmt_list_loop
qs_rt_fmt_list_end:
  add rsp, 32
  @APP:rbracket@
qs_rt_fmt_ret:
  pop rdx
  pop r12
  pop rbx
  ret

; rdi = value, rsi = type descriptor: prints one line on standard output
qs_rt_print:
  push rdi
  push rsi
  xor rax, rax
  mov [r13+@S_SLEN@], rax
  mov rdx, 0
  mov rdi, [rsp+8]
  mov rsi, [rsp]
  call qs_rt_fmt
  @APP:newline@
  mov rdi, 1
  mov rsi, [r13+@S_SPTR@]
  mov rdx, [r13+@S_SLEN@]
  call qs_rt_write
  add rsp, 16
  cmp rax, 0
  jne qs_rt_print_fail
  ret
qs_rt_print_fail:
  mov rdi, 127
  mov rax, 60
  syscall
  ud2

; ---- comparison and search ----
; rdi = a, rsi = b, rdx = count -> rax = 1 when the bytes are equal
qs_rt_eq:
  cmp rdx, 0
  je qs_rt_eq_yes
  mov r10, 0
  mov r10b, [rdi]
  mov r11, 0
  mov r11b, [rsi]
  cmp r10, r11
  jne qs_rt_eq_no
  add rdi, 1
  add rsi, 1
  sub rdx, 1
  jmp qs_rt_eq
qs_rt_eq_yes:
  mov rax, 1
  ret
qs_rt_eq_no:
  mov rax, 0
  ret

; rdi = a, rsi = an, rdx = b, rcx = bn -> rax = -1, 0 or 1 (bytewise, then by length)
qs_rt_cmpb:
  mov r8, rsi
  cmp rsi, rcx
  jbe qs_rt_cmpb_loop
  mov r8, rcx
qs_rt_cmpb_loop:
  cmp r8, 0
  je qs_rt_cmpb_len
  mov rax, 0
  mov al, [rdi]
  mov r10, 0
  mov r10b, [rdx]
  cmp rax, r10
  jb qs_rt_cmpb_less
  ja qs_rt_cmpb_more
  add rdi, 1
  add rdx, 1
  sub r8, 1
  jmp qs_rt_cmpb_loop
qs_rt_cmpb_len:
  cmp rsi, rcx
  jb qs_rt_cmpb_less
  ja qs_rt_cmpb_more
  mov rax, 0
  ret
qs_rt_cmpb_less:
  mov rax, -1
  ret
qs_rt_cmpb_more:
  mov rax, 1
  ret

; rdi = s, rsi = n, rdx = p, rcx = m, r8 = from -> rax = first offset >= from, or -1
qs_rt_find:
  cmp rcx, 0
  jne qs_rt_find_go
  mov rax, r8
  ret
qs_rt_find_go:
  push rdi
  push rsi
  push rdx
  push rcx
  push r8
qs_rt_find_loop:
  mov rax, [rsp]
  mov rcx, [rsp+8]
  add rax, rcx
  mov rdx, [rsp+24]
  cmp rax, rdx
  ja qs_rt_find_none
  mov rdi, [rsp+32]
  mov rax, [rsp]
  add rdi, rax
  mov rsi, [rsp+16]
  mov rdx, [rsp+8]
  call qs_rt_eq
  cmp rax, 0
  jne qs_rt_find_hit
  mov rax, [rsp]
  add rax, 1
  mov [rsp], rax
  jmp qs_rt_find_loop
qs_rt_find_hit:
  mov rax, [rsp]
  add rsp, 40
  ret
qs_rt_find_none:
  mov rax, -1
  add rsp, 40
  ret

; rdi = pointer, rsi = index -> rax = that byte
qs_rt_byte:
  mov rax, rdi
  add rax, rsi
  mov rcx, 0
  mov cl, [rax]
  mov rax, rcx
  ret

; rax = byte -> rax = 1 for space, tab, CR or LF (the characters integer parsing skips)
qs_rt_ws:
  cmp rax, 32
  je qs_rt_ws_yes
  cmp rax, 9
  je qs_rt_ws_yes
  cmp rax, 13
  je qs_rt_ws_yes
  cmp rax, 10
  je qs_rt_ws_yes
  mov rax, 0
  ret
qs_rt_ws_yes:
  mov rax, 1
  ret

; rax = byte -> rax = 1 for space, tab, CR, LF, form feed or vertical tab (trim)
qs_rt_trimc:
  cmp rax, 11
  je qs_rt_trimc_yes
  cmp rax, 12
  je qs_rt_trimc_yes
  cmp rax, 32
  je qs_rt_trimc_yes
  cmp rax, 9
  je qs_rt_trimc_yes
  cmp rax, 13
  je qs_rt_trimc_yes
  cmp rax, 10
  je qs_rt_trimc_yes
  mov rax, 0
  ret
qs_rt_trimc_yes:
  mov rax, 1
  ret

; rdi = pointer, rsi = length -> rax = value, rdx = 1 when it is a signed 64-bit integer
; (the grammar of metachain::text::signed_integer, as in CRuntime.cpp)
qs_rt_to_int:
  push rbx
  push r12
  mov r8, 0
  mov r9, rsi
  mov r10, 0
  mov r11, 10
qs_rt_to_int_lead:
  cmp r8, r9
  jae qs_rt_to_int_trail
  mov rsi, r8
  call qs_rt_byte
  call qs_rt_ws
  cmp rax, 0
  je qs_rt_to_int_trail
  add r8, 1
  jmp qs_rt_to_int_lead
qs_rt_to_int_trail:
  cmp r9, r8
  jbe qs_rt_to_int_sign
  mov rsi, r9
  sub rsi, 1
  call qs_rt_byte
  call qs_rt_ws
  cmp rax, 0
  je qs_rt_to_int_sign
  sub r9, 1
  jmp qs_rt_to_int_trail
qs_rt_to_int_sign:
  cmp r8, r9
  jae qs_rt_to_int_prefix
  mov rsi, r8
  call qs_rt_byte
  cmp rax, 43
  je qs_rt_to_int_plus
  cmp rax, 45
  je qs_rt_to_int_minus
  jmp qs_rt_to_int_prefix
qs_rt_to_int_plus:
  add r8, 1
  jmp qs_rt_to_int_prefix
qs_rt_to_int_minus:
  mov r10, 1
  add r8, 1
qs_rt_to_int_prefix:
  mov rax, r9
  sub rax, r8
  cmp rax, 2
  jb qs_rt_to_int_digits
  mov rsi, r8
  call qs_rt_byte
  cmp rax, 48
  jne qs_rt_to_int_digits
  mov rsi, r8
  add rsi, 1
  call qs_rt_byte
  cmp rax, 120
  je qs_rt_to_int_hex
  cmp rax, 88
  je qs_rt_to_int_hex
  cmp rax, 98
  je qs_rt_to_int_bin
  cmp rax, 66
  je qs_rt_to_int_bin
  cmp rax, 111
  je qs_rt_to_int_oct
  cmp rax, 79
  je qs_rt_to_int_oct
  jmp qs_rt_to_int_digits
qs_rt_to_int_hex:
  mov r11, 16
  add r8, 2
  jmp qs_rt_to_int_digits
qs_rt_to_int_bin:
  mov r11, 2
  add r8, 2
  jmp qs_rt_to_int_digits
qs_rt_to_int_oct:
  mov r11, 8
  add r8, 2
qs_rt_to_int_digits:
  cmp r8, r9
  jae qs_rt_to_int_fail
  mov rbx, 0
qs_rt_to_int_loop:
  cmp r8, r9
  jae qs_rt_to_int_range
  mov rsi, r8
  call qs_rt_byte
  cmp rax, 48
  jb qs_rt_to_int_fail
  cmp rax, 57
  jbe qs_rt_to_int_dec
  cmp rax, 97
  jb qs_rt_to_int_upper
  cmp rax, 122
  ja qs_rt_to_int_fail
  sub rax, 87
  jmp qs_rt_to_int_have
qs_rt_to_int_upper:
  cmp rax, 65
  jb qs_rt_to_int_fail
  cmp rax, 90
  ja qs_rt_to_int_fail
  sub rax, 55
  jmp qs_rt_to_int_have
qs_rt_to_int_dec:
  sub rax, 48
qs_rt_to_int_have:
  cmp rax, r11
  jae qs_rt_to_int_fail
  mov r12, rax
  mov rax, -1
  sub rax, r12
  xor rdx, rdx
  div r11
  cmp rbx, rax
  ja qs_rt_to_int_fail
  mov rax, rbx
  imul rax, r11
  add rax, r12
  mov rbx, rax
  add r8, 1
  jmp qs_rt_to_int_loop
qs_rt_to_int_range:
  mov rax, 9223372036854775807
  add rax, r10
  cmp rbx, rax
  ja qs_rt_to_int_fail
  cmp r10, 0
  je qs_rt_to_int_pos
  mov rax, 1
  shl rax, 63
  cmp rbx, rax
  jne qs_rt_to_int_neg
  jmp qs_rt_to_int_done
qs_rt_to_int_neg:
  mov rax, rbx
  neg rax
  jmp qs_rt_to_int_done
qs_rt_to_int_pos:
  mov rax, rbx
qs_rt_to_int_done:
  mov rdx, 1
  pop r12
  pop rbx
  ret
qs_rt_to_int_fail:
  mov rax, 0
  mov rdx, 0
  pop r12
  pop rbx
  ret

; ---- sorting ----
; rdi = index -> rax = element of the list being sorted
qs_sort_get:
  mov rax, rdi
  shl rax, 3
  mov rcx, [r13+@S_SORTA@]
  add rax, rcx
  mov rax, [rax]
  ret

; rdi = index, rsi = value
qs_sort_set:
  mov rax, rdi
  shl rax, 3
  mov rcx, [r13+@S_SORTA@]
  add rax, rcx
  mov [rax], rsi
  ret

; rdi = x, rsi = y -> rax = sign of x - y, ordering integers or strings by their bytes
qs_rt_cmp_elems:
  mov rax, [r13+@S_SORTI@]
  cmp rax, 0
  je qs_rt_cmp_strings
  cmp rdi, rsi
  jl qs_rt_cmp_less
  jg qs_rt_cmp_more
  mov rax, 0
  ret
qs_rt_cmp_strings:
  push rdi
  push rsi
  mov rdi, [rsp+8]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  push rax
  mov rdi, [rsp+8]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  pop rcx
  add rsp, 16
  mov rdi, [rcx+24]
  mov rsi, [rcx+8]
  mov rdx, [rax+24]
  mov rcx, [rax+8]
  jmp qs_rt_cmpb
qs_rt_cmp_less:
  mov rax, -1
  ret
qs_rt_cmp_more:
  mov rax, 1
  ret

; rdi = root, rsi = end: sifts the element at root down in the heap of [0, end)
qs_sift:
  push rbx
  push r12
  mov rbx, rdi
  mov r12, rsi
qs_sift_top:
  mov rax, rbx
  shl rax, 1
  add rax, 1
  cmp rax, r12
  jae qs_sift_done
  mov [r13+@S_CHILD@], rax
  mov rcx, rax
  add rcx, 1
  cmp rcx, r12
  jae qs_sift_root
  mov rdi, rax
  call qs_sort_get
  mov [r13+@S_SV1@], rax
  mov rdi, [r13+@S_CHILD@]
  add rdi, 1
  call qs_sort_get
  mov rsi, rax
  mov rdi, [r13+@S_SV1@]
  call qs_rt_cmp_elems
  cmp rax, 0
  jge qs_sift_root
  mov rax, [r13+@S_CHILD@]
  add rax, 1
  mov [r13+@S_CHILD@], rax
qs_sift_root:
  mov rdi, rbx
  call qs_sort_get
  mov [r13+@S_SV1@], rax
  mov rdi, [r13+@S_CHILD@]
  call qs_sort_get
  mov [r13+@S_SV2@], rax
  mov rsi, rax
  mov rdi, [r13+@S_SV1@]
  call qs_rt_cmp_elems
  cmp rax, 0
  jge qs_sift_done
  mov rdi, rbx
  mov rsi, [r13+@S_SV2@]
  call qs_sort_set
  mov rdi, [r13+@S_CHILD@]
  mov rsi, [r13+@S_SV1@]
  call qs_sort_set
  mov rbx, [r13+@S_CHILD@]
  jmp qs_sift_top
qs_sift_done:
  pop r12
  pop rbx
  ret

; ---- built-ins: dispatch ----
; rdi = IL frame base (register n at [rdi-8*(n+1)]), rsi = built-in id
qs_rt_builtin:
  mov rax, [rdi-8]
  mov [r13+@S_ARG0@], rax
  mov rax, [rdi-16]
  mov [r13+@S_ARG1@], rax
  mov rax, [rdi-24]
  mov [r13+@S_ARG2@], rax
  mov rax, rsi
  mov rcx, @BUILTIN_BASE@
  sub rax, rcx
  cmp rax, @NBUILTINS@
  jae qs_rt_unknown
  lea rcx, [rel qs_rt_table]
  shl rax, 3
  add rax, rcx
  mov rax, [rax]
  jmp rax
qs_rt_unknown:
  @TRAP:unknown@

qs_b_unsupported:
  @TRAP:unknown@

qs_b_Die:
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov rcx, [rax+8]
  push rcx
  mov rcx, [rax+24]
  push rcx
  xor rax, rax
  mov [r13+@S_SLEN@], rax
  @APP:prefix@
  mov rcx, [rsp+8]
  cmp rcx, 0
  jne qs_b_Die_text
  @APP:die@
  jmp qs_b_Die_newline
qs_b_Die_text:
  mov rsi, [rsp+8]
  mov rax, 400
  cmp rsi, rax
  jbe qs_b_Die_app
  mov rsi, rax
qs_b_Die_app:
  mov rdi, [rsp]
  call qs_rt_app
qs_b_Die_newline:
  @APP:newline@
  add rsp, 16
  mov rdi, 126
  jmp qs_rt_flush

qs_b_StrLiteral:
  mov rax, [r13+@S_ARG0@]
  cmp rax, 0
  jl qs_b_StrLiteral_bad
  cmp rax, @NSTR@
  jae qs_b_StrLiteral_bad
  mov rcx, rax
  shl rcx, 3
  mov rdx, [r13+@S_LIT@]
  add rcx, rdx
  mov rax, [rcx]
  cmp rax, 0
  jne qs_b_StrLiteral_done
  push rcx
  mov rdi, [r13+@S_ARG0@]
  shl rdi, 4
  lea rdx, [rel qs_strings]
  add rdx, rdi
  mov rsi, [rdx+8]
  mov rdi, [rdx]
  call qs_rt_str_copy
  pop rcx
  mov [rcx], rax
qs_b_StrLiteral_done:
  ret
qs_b_StrLiteral_bad:
  @TRAP:strlit@

qs_b_ListNew:
  mov rdi, @KIND_LIST@
  mov rsi, 0
  call qs_rt_alloc
  ret

qs_b_ListPush:
  push rbx
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_LIST@
  call qs_rt_desc
  mov rbx, rax
  mov rdi, 8
  call qs_rt_charge
  mov rsi, [rbx+8]
  add rsi, 1
  mov rdi, rbx
  call qs_rt_reserve
  mov rax, [rbx+8]
  mov rcx, rax
  shl rcx, 3
  mov rdx, [rbx+24]
  add rcx, rdx
  mov rdx, [r13+@S_ARG1@]
  mov [rcx], rdx
  add rax, 1
  mov [rbx+8], rax
  mov rax, 0
  pop rbx
  ret

qs_b_ListGet:
  push rbx
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_LIST@
  call qs_rt_desc
  mov rbx, rax
  mov rdi, [r13+@S_ARG1@]
  mov rsi, [rbx+8]
  call qs_rt_check_index
  mov rax, [r13+@S_ARG1@]
  shl rax, 3
  mov rcx, [rbx+24]
  add rax, rcx
  mov rax, [rax]
  pop rbx
  ret

qs_b_ListSet:
  push rbx
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_LIST@
  call qs_rt_desc
  mov rbx, rax
  mov rdi, [r13+@S_ARG1@]
  mov rsi, [rbx+8]
  call qs_rt_check_index
  mov rax, [r13+@S_ARG1@]
  shl rax, 3
  mov rcx, [rbx+24]
  add rax, rcx
  mov rcx, [r13+@S_ARG2@]
  mov [rax], rcx
  mov rax, 0
  pop rbx
  ret

qs_b_ListLen:
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_LIST@
  call qs_rt_desc
  mov rax, [rax+8]
  ret

qs_b_ListPop:
  push rbx
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_LIST@
  call qs_rt_desc
  mov rbx, rax
  mov rax, [rbx+8]
  cmp rax, 0
  je qs_b_ListPop_empty
  sub rax, 1
  mov [rbx+8], rax
  shl rax, 3
  mov rcx, [rbx+24]
  add rax, rcx
  mov rax, [rax]
  pop rbx
  ret
qs_b_ListPop_empty:
  @TRAP:popempty@

qs_b_ListLast:
  push rbx
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_LIST@
  call qs_rt_desc
  mov rbx, rax
  mov rax, [rbx+8]
  cmp rax, 0
  je qs_b_ListLast_empty
  sub rax, 1
  shl rax, 3
  mov rcx, [rbx+24]
  add rax, rcx
  mov rax, [rax]
  pop rbx
  ret
qs_b_ListLast_empty:
  @TRAP:lastempty@

qs_b_ListContains:
  push rbx
  push r12
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_LIST@
  call qs_rt_desc
  mov rbx, rax
  mov rdi, [r13+@S_ARG2@]
  call qs_rt_kind_of
  cmp rax, @KIND_INT@
  jne qs_b_ListContains_strings
  mov r12, 0
qs_b_ListContains_ints:
  mov rax, r12
  mov rcx, [rbx+8]
  cmp rax, rcx
  jae qs_b_ListContains_no
  mov rax, r12
  shl rax, 3
  mov rcx, [rbx+24]
  add rax, rcx
  mov rax, [rax]
  mov rcx, [r13+@S_ARG1@]
  cmp rax, rcx
  je qs_b_ListContains_yes
  add r12, 1
  jmp qs_b_ListContains_ints
qs_b_ListContains_strings:
  mov r12, 0
qs_b_ListContains_loop:
  mov rax, r12
  mov rcx, [rbx+8]
  cmp rax, rcx
  jae qs_b_ListContains_no
  mov rax, r12
  shl rax, 3
  mov rcx, [rbx+24]
  add rax, rcx
  mov rdi, [rax]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  push rax
  mov rdi, [r13+@S_ARG1@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  pop rcx
  mov rdx, [rcx+8]
  mov r8, [rax+8]
  cmp rdx, r8
  jne qs_b_ListContains_next
  mov rdi, [rcx+24]
  mov rsi, [rax+24]
  call qs_rt_eq
  cmp rax, 0
  jne qs_b_ListContains_yes
qs_b_ListContains_next:
  add r12, 1
  jmp qs_b_ListContains_loop
qs_b_ListContains_yes:
  mov rax, 1
  pop r12
  pop rbx
  ret
qs_b_ListContains_no:
  mov rax, 0
  pop r12
  pop rbx
  ret

qs_b_ListSort:
  push rbx
  push r12
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_LIST@
  call qs_rt_desc
  mov rbx, rax
  mov rdi, [r13+@S_ARG1@]
  call qs_rt_kind_of
  mov rcx, 0
  cmp rax, @KIND_INT@
  jne qs_b_ListSort_kind
  mov rcx, 1
qs_b_ListSort_kind:
  mov [r13+@S_SORTI@], rcx
  mov rax, [rbx+8]
  cmp rax, 1
  jbe qs_b_ListSort_done
  mov [r13+@S_SORTN@], rax
  mov rcx, [rbx+24]
  mov [r13+@S_SORTA@], rcx
  shr rax, 1
  mov r12, rax
qs_b_ListSort_heapify:
  cmp r12, 0
  je qs_b_ListSort_extract
  sub r12, 1
  mov rdi, r12
  mov rsi, [r13+@S_SORTN@]
  call qs_sift
  jmp qs_b_ListSort_heapify
qs_b_ListSort_extract:
  mov r12, [r13+@S_SORTN@]
  sub r12, 1
qs_b_ListSort_loop:
  cmp r12, 0
  je qs_b_ListSort_done
  mov rdi, 0
  call qs_sort_get
  mov [r13+@S_SV1@], rax
  mov rdi, r12
  call qs_sort_get
  mov [r13+@S_SV2@], rax
  mov rdi, 0
  mov rsi, [r13+@S_SV2@]
  call qs_sort_set
  mov rdi, r12
  mov rsi, [r13+@S_SV1@]
  call qs_sort_set
  mov rdi, 0
  mov rsi, r12
  call qs_sift
  sub r12, 1
  jmp qs_b_ListSort_loop
qs_b_ListSort_done:
  mov rax, 0
  pop r12
  pop rbx
  ret

qs_b_ListReverse:
  push rbx
  push r12
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_LIST@
  call qs_rt_desc
  mov rbx, rax
  mov r12, 0
  mov r8, [rbx+8]
  cmp r8, 0
  je qs_b_ListReverse_done
  sub r8, 1
qs_b_ListReverse_loop:
  cmp r12, r8
  jae qs_b_ListReverse_done
  mov rax, r12
  shl rax, 3
  mov rcx, [rbx+24]
  add rax, rcx
  mov rdx, r8
  shl rdx, 3
  add rdx, rcx
  mov rsi, [rax]
  mov rdi, [rdx]
  mov [rax], rdi
  mov [rdx], rsi
  add r12, 1
  sub r8, 1
  jmp qs_b_ListReverse_loop
qs_b_ListReverse_done:
  mov rax, 0
  pop r12
  pop rbx
  ret

qs_b_ListClear:
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_LIST@
  call qs_rt_desc
  mov rcx, 0
  mov [rax+8], rcx
  mov rax, 0
  ret

qs_b_ListJoin:
  push rbx
  push r12
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_LIST@
  call qs_rt_desc
  mov rbx, rax
  mov rdi, [r13+@S_ARG1@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov r12, rax
  xor rax, rax
  mov [r13+@S_SLEN@], rax
  push rax
qs_b_ListJoin_loop:
  mov rax, [rsp]
  mov rcx, [rbx+8]
  cmp rax, rcx
  jae qs_b_ListJoin_end
  cmp rax, 0
  je qs_b_ListJoin_item
  mov rdi, [r12+24]
  mov rsi, [r12+8]
  call qs_rt_app
qs_b_ListJoin_item:
  mov rax, [rsp]
  shl rax, 3
  mov rcx, [rbx+24]
  add rax, rcx
  mov rdi, [rax]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov rdi, [rax+24]
  mov rsi, [rax+8]
  call qs_rt_app
  mov rax, [rsp]
  add rax, 1
  mov [rsp], rax
  jmp qs_b_ListJoin_loop
qs_b_ListJoin_end:
  add rsp, 8
  mov rdi, [r13+@S_SPTR@]
  mov rsi, [r13+@S_SLEN@]
  call qs_rt_str_copy
  pop r12
  pop rbx
  ret

qs_b_ListSlice:
  push rbx
  push r12
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_LIST@
  call qs_rt_desc
  mov rbx, rax
  mov rax, [r13+@S_ARG1@]
  mov rcx, [r13+@S_ARG2@]
  cmp rax, 0
  jl qs_b_ListSlice_bad
  cmp rcx, 0
  jl qs_b_ListSlice_bad
  mov rdx, [rbx+8]
  cmp rax, rdx
  ja qs_b_ListSlice_bad
  mov r8, rdx
  sub r8, rax
  cmp rcx, r8
  ja qs_b_ListSlice_bad
  mov r12, rcx
  mov rdi, @KIND_LIST@
  mov rsi, r12
  shl rsi, 3
  call qs_rt_alloc
  push rax
  mov rdi, rax
  call qs_rt_slot
  mov rdi, rax
  mov rsi, r12
  call qs_rt_reserve
  mov rdi, [rsp]
  call qs_rt_slot
  mov rcx, rax
  mov [rcx+8], r12
  mov rdi, [rcx+24]
  mov rsi, [rbx+24]
  mov rax, [r13+@S_ARG1@]
  shl rax, 3
  add rsi, rax
  mov rdx, r12
  shl rdx, 3
  call qs_rt_copy
  pop rax
  pop r12
  pop rbx
  ret
qs_b_ListSlice_bad:
  @TRAP:slice@

qs_b_StrCompare:
  push rbx
  push r12
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov rbx, rax
  mov rdi, [r13+@S_ARG1@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov r12, rax
  mov rdi, [rbx+24]
  mov rsi, [rbx+8]
  mov rdx, [r12+24]
  mov rcx, [r12+8]
  call qs_rt_cmpb
  pop r12
  pop rbx
  ret

qs_b_StrConcat:
  push rbx
  push r12
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov rbx, rax
  mov rdi, [r13+@S_ARG1@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov r12, rax
  mov rax, [rbx+8]
  mov rcx, [r12+8]
  add rax, rcx
  mov rdi, rax
  call qs_rt_str_new
  push rax
  mov rdi, rax
  call qs_rt_slot
  mov rcx, [rax+24]
  mov r8, [rbx+8]
  mov rdi, rcx
  mov rsi, [rbx+24]
  mov rdx, r8
  call qs_rt_copy
  mov rdi, rcx
  add rdi, r8
  mov rsi, [r12+24]
  mov rdx, [r12+8]
  call qs_rt_copy
  pop rax
  pop r12
  pop rbx
  ret

qs_b_StrLen:
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov rax, [rax+8]
  ret

qs_b_StrAt:
  push rbx
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov rbx, rax
  mov rdi, [r13+@S_ARG1@]
  mov rsi, [rbx+8]
  call qs_rt_check_index
  mov rax, [r13+@S_ARG1@]
  mov rcx, [rbx+24]
  add rax, rcx
  mov rcx, rax
  mov rax, 0
  mov al, [rcx]
  pop rbx
  ret

qs_b_StrSub:
  push rbx
  push r12
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov rbx, rax
  mov rax, [r13+@S_ARG1@]
  mov rcx, [r13+@S_ARG2@]
  cmp rax, 0
  jl qs_b_StrSub_bad
  cmp rcx, 0
  jl qs_b_StrSub_bad
  mov rdx, [rbx+8]
  cmp rax, rdx
  ja qs_b_StrSub_bad
  mov r8, rdx
  sub r8, rax
  cmp rcx, r8
  ja qs_b_StrSub_bad
  mov rdi, [rbx+24]
  add rdi, rax
  mov rsi, rcx
  call qs_rt_str_copy
  pop r12
  pop rbx
  ret
qs_b_StrSub_bad:
  @TRAP:substr@

qs_b_StrFind:
  push rbx
  push r12
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov rbx, rax
  mov rdi, [r13+@S_ARG1@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov r12, rax
  mov rdi, [rbx+24]
  mov rsi, [rbx+8]
  mov rdx, [r12+24]
  mov rcx, [r12+8]
  mov r8, 0
  call qs_rt_find
  pop r12
  pop rbx
  ret

qs_b_StrFindFrom:
  push rbx
  push r12
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov rbx, rax
  mov rdi, [r13+@S_ARG1@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov r12, rax
  mov r8, [r13+@S_ARG2@]
  cmp r8, 0
  jl qs_b_StrFindFrom_bad
  mov rax, [rbx+8]
  cmp r8, rax
  ja qs_b_StrFindFrom_bad
  mov rdi, [rbx+24]
  mov rsi, [rbx+8]
  mov rdx, [r12+24]
  mov rcx, [r12+8]
  call qs_rt_find
  pop r12
  pop rbx
  ret
qs_b_StrFindFrom_bad:
  @TRAP:searchpos@

qs_b_StrStarts:
  push rbx
  push r12
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov rbx, rax
  mov rdi, [r13+@S_ARG1@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov r12, rax
  mov rcx, [r12+8]
  mov rax, [rbx+8]
  cmp rax, rcx
  jb qs_b_StrStarts_no
  cmp rcx, 0
  je qs_b_StrStarts_yes
  mov rdi, [rbx+24]
  mov rsi, [r12+24]
  mov rdx, rcx
  call qs_rt_eq
  pop r12
  pop rbx
  ret
qs_b_StrStarts_yes:
  mov rax, 1
  pop r12
  pop rbx
  ret
qs_b_StrStarts_no:
  mov rax, 0
  pop r12
  pop rbx
  ret

qs_b_StrEnds:
  push rbx
  push r12
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov rbx, rax
  mov rdi, [r13+@S_ARG1@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov r12, rax
  mov rcx, [r12+8]
  mov rax, [rbx+8]
  cmp rax, rcx
  jb qs_b_StrEnds_no
  cmp rcx, 0
  je qs_b_StrEnds_yes
  sub rax, rcx
  mov rdi, [rbx+24]
  add rdi, rax
  mov rsi, [r12+24]
  mov rdx, rcx
  call qs_rt_eq
  pop r12
  pop rbx
  ret
qs_b_StrEnds_yes:
  mov rax, 1
  pop r12
  pop rbx
  ret
qs_b_StrEnds_no:
  mov rax, 0
  pop r12
  pop rbx
  ret

qs_b_StrContains:
  push rbx
  push r12
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov rbx, rax
  mov rdi, [r13+@S_ARG1@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov r12, rax
  mov rdi, [rbx+24]
  mov rsi, [rbx+8]
  mov rdx, [r12+24]
  mov rcx, [r12+8]
  mov r8, 0
  call qs_rt_find
  cmp rax, 0
  jl qs_b_StrContains_no
  mov rax, 1
  pop r12
  pop rbx
  ret
qs_b_StrContains_no:
  mov rax, 0
  pop r12
  pop rbx
  ret

qs_b_StrSplit:
  push rbx
  push r12
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov rbx, rax
  mov rdi, [r13+@S_ARG1@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov r12, rax
  mov rax, [r12+8]
  cmp rax, 0
  je qs_b_StrSplit_empty
  mov rax, 1
  mov [r13+@S_SPLCNT@], rax
  mov rax, 0
  mov [r13+@S_SPLSTART@], rax
qs_b_StrSplit_count:
  mov rdi, [rbx+24]
  mov rsi, [rbx+8]
  mov rdx, [r12+24]
  mov rcx, [r12+8]
  mov r8, [r13+@S_SPLSTART@]
  call qs_rt_find
  cmp rax, 0
  jl qs_b_StrSplit_counted
  mov rcx, [r12+8]
  add rax, rcx
  mov [r13+@S_SPLSTART@], rax
  mov rax, [r13+@S_SPLCNT@]
  add rax, 1
  mov [r13+@S_SPLCNT@], rax
  jmp qs_b_StrSplit_count
qs_b_StrSplit_counted:
  mov rdi, @KIND_LIST@
  mov rsi, [r13+@S_SPLCNT@]
  shl rsi, 3
  call qs_rt_alloc
  mov [r13+@S_SPLH@], rax
  mov rdi, rax
  call qs_rt_slot
  mov rdi, rax
  mov rsi, [r13+@S_SPLCNT@]
  call qs_rt_reserve
  mov rax, 0
  mov [r13+@S_SPLSTART@], rax
  mov [r13+@S_SPLK@], rax
qs_b_StrSplit_fill:
  mov rdi, [rbx+24]
  mov rsi, [rbx+8]
  mov rdx, [r12+24]
  mov rcx, [r12+8]
  mov r8, [r13+@S_SPLSTART@]
  call qs_rt_find
  mov [r13+@S_SPLFOUND@], rax
  mov rcx, [rbx+8]
  cmp rax, 0
  jl qs_b_StrSplit_have_end
  mov rcx, rax
qs_b_StrSplit_have_end:
  mov rsi, [r13+@S_SPLSTART@]
  sub rcx, rsi
  mov rdi, [rbx+24]
  add rdi, rsi
  mov rsi, rcx
  call qs_rt_str_copy
  mov [r13+@S_SPLPIECE@], rax
  mov rdi, [r13+@S_SPLH@]
  call qs_rt_slot
  mov rdx, [rax+24]
  mov rsi, [r13+@S_SPLK@]
  shl rsi, 3
  add rdx, rsi
  mov rcx, [r13+@S_SPLPIECE@]
  mov [rdx], rcx
  mov rax, [r13+@S_SPLK@]
  add rax, 1
  mov [r13+@S_SPLK@], rax
  mov rax, [r13+@S_SPLFOUND@]
  cmp rax, 0
  jl qs_b_StrSplit_done
  mov rcx, [r12+8]
  add rax, rcx
  mov [r13+@S_SPLSTART@], rax
  jmp qs_b_StrSplit_fill
qs_b_StrSplit_done:
  mov rdi, [r13+@S_SPLH@]
  call qs_rt_slot
  mov rcx, [r13+@S_SPLCNT@]
  mov [rax+8], rcx
  mov rax, [r13+@S_SPLH@]
  pop r12
  pop rbx
  ret
qs_b_StrSplit_empty:
  @TRAP:splitempty@

qs_b_StrRepeat:
  push rbx
  push r12
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov rbx, rax
  mov r12, [r13+@S_ARG1@]
  cmp r12, 0
  jl qs_b_StrRepeat_negative
  mov r8, [rbx+8]
  cmp r12, 0
  je qs_b_StrRepeat_make
  mov rax, @MAX_BYTES@
  xor rdx, rdx
  mov rcx, r12
  div rcx
  cmp r8, rax
  ja qs_b_StrRepeat_limit
qs_b_StrRepeat_make:
  mov rax, r8
  imul rax, r12
  mov rdi, rax
  call qs_rt_str_new
  push rax
  mov rdi, rax
  call qs_rt_slot
  mov rcx, [rax+24]
  mov [r13+@S_RPDST@], rcx
  mov rax, 0
  mov [r13+@S_RPI@], rax
qs_b_StrRepeat_loop:
  mov rax, [r13+@S_RPI@]
  cmp rax, r12
  jae qs_b_StrRepeat_done
  mov rcx, [rbx+8]
  cmp rcx, 0
  je qs_b_StrRepeat_done
  mov rdi, [r13+@S_RPDST@]
  mov rsi, [rbx+24]
  mov rdx, [rbx+8]
  call qs_rt_copy
  mov rax, [r13+@S_RPDST@]
  mov rcx, [rbx+8]
  add rax, rcx
  mov [r13+@S_RPDST@], rax
  mov rax, [r13+@S_RPI@]
  add rax, 1
  mov [r13+@S_RPI@], rax
  jmp qs_b_StrRepeat_loop
qs_b_StrRepeat_done:
  pop rax
  pop r12
  pop rbx
  ret
qs_b_StrRepeat_negative:
  @TRAP:repneg@
qs_b_StrRepeat_limit:
  @LIMIT:heap_bytes@

qs_b_StrToInt:
  push rbx
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov rbx, rax
  mov rdi, [rbx+24]
  mov rsi, [rbx+8]
  call qs_rt_to_int
  cmp rdx, 0
  je qs_b_StrToInt_bad
  pop rbx
  ret
qs_b_StrToInt_bad:
  xor rax, rax
  mov [r13+@S_SLEN@], rax
  @APP:prefix@
  @APP:notint@
  @APP:quote@
  mov rdi, [rbx+24]
  mov rsi, [rbx+8]
  mov rax, 300
  cmp rsi, rax
  jbe qs_b_StrToInt_app
  mov rsi, rax
qs_b_StrToInt_app:
  call qs_rt_app
  @APP:quote@
  @APP:newline@
  mov rdi, 126
  jmp qs_rt_flush

qs_rt_case:
  push rbx
  push r12
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov rbx, rax
  mov rdi, [rbx+8]
  call qs_rt_str_new
  mov r12, rax
  mov rdi, rax
  call qs_rt_slot
  mov rdi, [rax+24]
  mov rsi, [rbx+24]
  mov rdx, [rbx+8]
qs_rt_case_loop:
  cmp rdx, 0
  je qs_rt_case_done
  mov rax, 0
  mov al, [rsi]
  mov rcx, [rsp+16]
  cmp rcx, 0
  je qs_rt_case_lower
  cmp rax, 97
  jb qs_rt_case_store
  cmp rax, 122
  ja qs_rt_case_store
  sub rax, 32
  jmp qs_rt_case_store
qs_rt_case_lower:
  cmp rax, 65
  jb qs_rt_case_store
  cmp rax, 90
  ja qs_rt_case_store
  add rax, 32
qs_rt_case_store:
  mov [rdi], al
  add rdi, 1
  add rsi, 1
  sub rdx, 1
  jmp qs_rt_case_loop
qs_rt_case_done:
  mov rax, r12
  pop r12
  pop rbx
  add rsp, 8
  ret

qs_b_StrUpper:
  mov rax, 1
  push rax
  jmp qs_rt_case

qs_b_StrLower:
  mov rax, 0
  push rax
  jmp qs_rt_case

qs_b_StrTrim:
  push rbx
  push r12
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov rbx, rax
  mov r8, 0
  mov r9, [rbx+8]
qs_b_StrTrim_lead:
  cmp r8, r9
  jae qs_b_StrTrim_empty
  mov rdi, [rbx+24]
  mov rsi, r8
  call qs_rt_byte
  call qs_rt_trimc
  cmp rax, 0
  je qs_b_StrTrim_trail_start
  add r8, 1
  jmp qs_b_StrTrim_lead
qs_b_StrTrim_trail_start:
  mov r10, r9
qs_b_StrTrim_trail:
  cmp r10, r8
  jbe qs_b_StrTrim_make
  mov rdi, [rbx+24]
  mov rsi, r10
  sub rsi, 1
  call qs_rt_byte
  call qs_rt_trimc
  cmp rax, 0
  je qs_b_StrTrim_make
  sub r10, 1
  jmp qs_b_StrTrim_trail
qs_b_StrTrim_make:
  mov rdi, [rbx+24]
  add rdi, r8
  mov rsi, r10
  sub rsi, r8
  call qs_rt_str_copy
  pop r12
  pop rbx
  ret
qs_b_StrTrim_empty:
  mov rdi, 0
  mov rsi, 0
  call qs_rt_str_copy
  pop r12
  pop rbx
  ret

qs_b_StrReplace:
  push rbx
  push r12
  mov rdi, [r13+@S_ARG0@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov rbx, rax
  mov rdi, [r13+@S_ARG1@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov r12, rax
  mov rax, [r12+8]
  cmp rax, 0
  je qs_b_StrReplace_empty
  mov rdi, [r13+@S_ARG2@]
  mov rsi, @KIND_STR@
  call qs_rt_desc
  mov [r13+@S_RTO@], rax
  mov rax, 0
  mov [r13+@S_SLEN@], rax
  mov [r13+@S_RSTART@], rax
qs_b_StrReplace_loop:
  mov rdi, [rbx+24]
  mov rsi, [rbx+8]
  mov rdx, [r12+24]
  mov rcx, [r12+8]
  mov r8, [r13+@S_RSTART@]
  call qs_rt_find
  cmp rax, 0
  jl qs_b_StrReplace_tail
  mov [r13+@S_RFOUND@], rax
  mov rcx, [r13+@S_RSTART@]
  mov rdi, [rbx+24]
  add rdi, rcx
  mov rsi, rax
  sub rsi, rcx
  call qs_rt_app
  mov rax, [r13+@S_RTO@]
  mov rdi, [rax+24]
  mov rsi, [rax+8]
  call qs_rt_app
  mov rax, [r13+@S_RFOUND@]
  mov rcx, [r12+8]
  add rax, rcx
  mov [r13+@S_RSTART@], rax
  jmp qs_b_StrReplace_loop
qs_b_StrReplace_tail:
  mov rcx, [r13+@S_RSTART@]
  mov rdi, [rbx+24]
  add rdi, rcx
  mov rsi, [rbx+8]
  sub rsi, rcx
  call qs_rt_app
  mov rdi, [r13+@S_SPTR@]
  mov rsi, [r13+@S_SLEN@]
  call qs_rt_str_copy
  pop r12
  pop rbx
  ret
qs_b_StrReplace_empty:
  @TRAP:replaceempty@

qs_b_IntToStr:
  xor rax, rax
  mov [r13+@S_SLEN@], rax
  mov rdi, [r13+@S_ARG0@]
  call qs_rt_app_int
  mov rdi, [r13+@S_SPTR@]
  mov rsi, [r13+@S_SLEN@]
  call qs_rt_str_copy
  ret

qs_b_IntToChar:
  mov rax, [r13+@S_ARG0@]
  cmp rax, 0
  jl qs_b_IntToChar_bad
  cmp rax, 255
  ja qs_b_IntToChar_bad
  push rax
  mov rdi, 1
  call qs_rt_str_new
  push rax
  mov rdi, rax
  call qs_rt_slot
  mov rcx, [rax+24]
  mov rdx, [rsp+8]
  mov [rcx], dl
  pop rax
  add rsp, 8
  ret
qs_b_IntToChar_bad:
  @TRAP:chr@

qs_b_PrintValue:
  mov rdi, [r13+@S_ARG0@]
  mov rsi, [r13+@S_ARG1@]
  call qs_rt_print
  mov rax, 0
  ret
)QSASM";

struct Message { const char* name; const char* text; };
// Fixed texts. Each name is the label suffix (qs_m_<name>); the messages match
// CRuntime.cpp, so both run-times report the same failures.
constexpr Message messages[] = {
    {"prefix", "qscript: "}, {"newline", "\n"}, {"quote", "\""}, {"comma", ", "},
    {"lbracket", "["}, {"rbracket", "]"}, {"minus", "-"}, {"die", "die"},
    {"index", "index "}, {"outrange", " out of range (length "}, {"rparen_nl", ")\n"},
    {"invalid", "internal: invalid QScript value"}, {"badtype", "internal: type descriptor out of range"},
    {"badprint", "internal: value cannot be printed by the native backend"},
    {"strlit", "internal: string literal out of range"}, {"unknown", "internal: unknown built-in"},
    {"popempty", "pop from an empty list"}, {"lastempty", "last of an empty list"},
    {"slice", "slice out of range"}, {"substr", "substring out of range"},
    {"searchpos", "search position out of range"}, {"splitempty", "split separator must not be empty"},
    {"repneg", "repeat count must not be negative"}, {"notint", "not a signed 64-bit integer: "},
    {"replaceempty", "replace pattern must not be empty"}, {"chr", "character code out of range 0..255"},
    {"heap_bytes", "heap size limit exceeded"}, {"objects", "heap object limit exceeded"},
    {"oom", "out of memory"},
};

// Expands the tokens of `text`: @NAME@ becomes a constant; @APP:m@, @TRAP:m@ and
// @LIMIT:m@ become the calls that append, or fail with, the message m.
std::string expand(std::string_view text, const std::map<std::string, std::string>& constants,
                   const std::map<std::string, std::size_t>& lengths) {
    std::string out;
    for (std::size_t i = 0; i < text.size();) {
        if (text[i] != '@') { out += text[i++]; continue; }
        const auto end = text.find('@', i + 1);
        if (end == std::string_view::npos) throw std::logic_error("unterminated token in native run-time");
        const std::string_view token = text.substr(i + 1, end - i - 1);
        i = end + 1;
        const auto colon = token.find(':');
        if (colon == std::string_view::npos) {
            out += constants.at(std::string(token));
            continue;
        }
        const std::string name(token.substr(colon + 1));
        const std::string op(token.substr(0, colon));
        out += "lea rdi, [rel qs_m_" + name + "]\nmov rsi, " + std::to_string(lengths.at(name)) + '\n';
        if (op == "APP") out += "call qs_rt_app\n";
        else if (op == "TRAP") out += "jmp qs_rt_trap\n";
        else if (op == "LIMIT") out += "jmp qs_rt_limit\n";
        else throw std::logic_error("unknown token in native run-time: " + op);
    }
    return out;
}

// `.ascii` lines that spell `bytes` exactly, with every byte escaped as \xHH.
void ascii(std::string& out, std::string_view bytes) {
    constexpr std::size_t line = 32;
    static constexpr char hex[] = "0123456789abcdef";
    for (std::size_t start = 0; start < bytes.size(); start += line) {
        out += "    .ascii \"";
        for (std::size_t i = start; i < bytes.size() && i < start + line; ++i) {
            const auto c = static_cast<unsigned char>(bytes[i]);
            out += "\\x";
            out += hex[c >> 4];
            out += hex[c & 15];
        }
        out += "\"\n";
    }
}

} // namespace

std::string native_runtime_text(const std::vector<std::string>& strings, const std::vector<RunTimeType>& types,
                                std::uint64_t max_objects, std::uint64_t max_bytes) {
    constexpr std::uint64_t state_size = 4096;
    constexpr std::uint64_t chunk = 1u << 20;
    constexpr std::uint64_t descriptor = 32;
    // Field offsets of the state block (r13); see the comment on the routines.
    const std::map<std::string, std::string> constants = {
        {"STATE_SIZE", std::to_string(state_size)},
        {"CHUNK", std::to_string(chunk)},
        {"S_ARG0", "0"}, {"S_ARG1", "8"}, {"S_ARG2", "16"}, {"S_DESC", "24"}, {"S_COUNT", "32"},
        {"S_LIT", "40"}, {"S_CUR", "48"}, {"S_END", "56"}, {"S_USED", "64"}, {"S_SPTR", "72"},
        {"S_SLEN", "80"}, {"S_SCAP", "88"}, {"S_GTMP", "96"}, {"S_SORTA", "104"}, {"S_SORTN", "112"},
        {"S_SORTI", "120"}, {"S_CHILD", "128"}, {"S_SV1", "136"}, {"S_SV2", "144"}, {"S_SPLCNT", "152"},
        {"S_SPLSTART", "160"}, {"S_SPLH", "168"}, {"S_SPLK", "176"}, {"S_SPLFOUND", "184"},
        {"S_SPLPIECE", "192"}, {"S_RPDST", "200"}, {"S_RPI", "208"}, {"S_RTO", "216"},
        {"S_RSTART", "224"}, {"S_RFOUND", "232"},
        {"KIND_INT", std::to_string(static_cast<int>(Type::Kind::Int))},
        {"KIND_STR", std::to_string(static_cast<int>(Type::Kind::Str))},
        {"KIND_LIST", std::to_string(static_cast<int>(Type::Kind::List))},
        {"MAX_OBJECTS", std::to_string(max_objects)},
        {"MAX_BYTES", std::to_string(max_bytes)},
        {"DESC_BYTES", std::to_string(std::max<std::uint64_t>(descriptor, max_objects * descriptor))},
        {"LIT_BYTES", std::to_string(std::max<std::uint64_t>(8, strings.size() * 8))},
        {"NTYPES", std::to_string(types.size())},
        {"NSTR", std::to_string(strings.size())},
        {"BUILTIN_BASE", std::to_string(builtin_base)},
    };
    // Dispatch table indexed by the built-in number; numbers without a routine trap.
    std::size_t builtins = 0;
    for (const auto& b : runtime_builtins) builtins = std::max(builtins, static_cast<std::size_t>(b.id) + 1);
    std::vector<std::string> routine(builtins, "qs_b_unsupported");
    for (const auto& b : runtime_builtins) routine[static_cast<std::size_t>(b.id)] = std::string("qs_b_") + b.name;
    std::map<std::string, std::string> all = constants;
    all["NBUILTINS"] = std::to_string(builtins);

    std::map<std::string, std::size_t> lengths;
    for (const auto& m : messages) lengths[m.name] = std::string_view(m.text).size();

    std::string out = expand(routines, all, lengths);
    out += "\n; ---- tables and messages ----\n    .align 8\nqs_types:\n";
    for (const auto& t : types) out += "    .quad " + std::to_string(t.kind) + "\n    .quad " + std::to_string(t.elem) + "\n";
    out += "qs_strings:\n";
    for (std::size_t i = 0; i < strings.size(); ++i)
        out += "    .quad qs_s_" + std::to_string(i) + "\n    .quad " + std::to_string(strings[i].size()) + "\n";
    out += "qs_rt_table:\n";
    for (const auto& r : routine) out += "    .quad " + r + "\n";
    for (const auto& m : messages) {
        out += std::string("qs_m_") + m.name + ":\n";
        ascii(out, m.text);
    }
    for (std::size_t i = 0; i < strings.size(); ++i) {
        out += "qs_s_" + std::to_string(i) + ":\n";
        ascii(out, strings[i]);
    }
    return out;
}

} // namespace qscript::detail
