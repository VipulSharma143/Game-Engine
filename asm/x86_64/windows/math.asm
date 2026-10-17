; Windows x64 ABI routines. NASM syntax, assembled with -f win64.
; Arguments: rcx, rdx. Return: rax. No callee-saved registers, stack or shadow space used.

default rel
bits 64

section .text
global ge_windows_add_i64
global ge_windows_multiply_i64

; int64_t ge_windows_add_i64(int64_t a, int64_t b)   -- wraps on overflow
ge_windows_add_i64:
    mov rax, rcx
    add rax, rdx
    ret

; int64_t ge_windows_multiply_i64(int64_t a, int64_t b)   -- low 64 bits of the product
ge_windows_multiply_i64:
    mov rax, rcx
    imul rax, rdx
    ret
