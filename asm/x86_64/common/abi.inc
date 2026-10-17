; Shared ABI helpers for routines that are assembled on BOTH Linux and Windows.
; Integer/pointer argument registers differ per ABI; the macros below map them.
;
;   System V AMD64 (elf64): args rdi, rsi, rdx, rcx
;   Windows x64   (win64) : args rcx, rdx, r8,  r9
;
; Routines written with these macros may freely use rax, r10, r11 and xmm0-xmm5
; (volatile on both ABIs) and must not touch rbx, rbp, rsi/rdi on Windows,
; r12-r15 or xmm6-xmm15. They use no stack, so no shadow space or alignment is needed.

default rel
bits 64

%ifidn __OUTPUT_FORMAT__, win64
  %define A1  rcx
  %define A2  rdx
  %define A3  r8
  %define A4  r9
  %define A1D ecx
  %define A2D edx
  %define A3D r8d
  %define A4D r9d
%elifidn __OUTPUT_FORMAT__, elf64
  %define A1  rdi
  %define A2  rsi
  %define A3  rdx
  %define A4  rcx
  %define A1D edi
  %define A2D esi
  %define A3D edx
  %define A4D ecx
%else
  %error "unsupported NASM output format (need elf64 or win64)"
%endif

%macro GE_FUNC 1
  section .text
  %ifidn __OUTPUT_FORMAT__, elf64
    global %1:function
  %else
    global %1
  %endif
%1:
%endmacro

; ELF only: mark the stack as non-executable.
%macro GE_END 0
  %ifidn __OUTPUT_FORMAT__, elf64
    section .note.GNU-stack noalloc noexec nowrite progbits
  %endif
%endmacro
