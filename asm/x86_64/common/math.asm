; Shared x86-64 math routines. NASM syntax.
; ABI: System V AMD64 ONLY (arguments in rdi, rsi). Assembled on Linux builds.
; Windows builds use asm/x86_64/windows/math.asm instead (arguments in rcx, rdx).
; Uses only baseline x86-64 instructions, so no CPU feature check is required.

default rel
bits 64

section .text
global ge_add_i64:function

; int64_t ge_add_i64(int64_t a, int64_t b)   -- wraps on overflow
ge_add_i64:
    mov rax, rdi
    add rax, rsi
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
