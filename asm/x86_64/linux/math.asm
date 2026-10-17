; Linux x86-64 System V ABI routines. NASM syntax.
; Arguments: rdi, rsi. Return: rax. No callee-saved registers or stack are touched.

default rel
bits 64

section .text
global ge_linux_multiply_i64:function

; int64_t ge_linux_multiply_i64(int64_t a, int64_t b)   -- low 64 bits of the product
ge_linux_multiply_i64:
    mov rax, rdi
    imul rax, rsi
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
