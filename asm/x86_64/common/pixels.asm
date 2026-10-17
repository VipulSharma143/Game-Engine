; Pixel/framebuffer routines, assembled on Linux (System V) and Windows (x64).
; Baseline x86-64 only: SSE2 (guaranteed on every x86-64 CPU), so no CPU feature check.
; Every routine has a C++ reference in engine/optimized/ScalarFallback.cpp.

%include "abi.inc"

; void ge_fill_u32(uint32_t* dst, uint32_t value, size_t count)
;   Writes `value` to dst[0..count). dst may be unaligned. Touches nothing else.
GE_FUNC ge_fill_u32
    movd    xmm0, A2D
    pshufd  xmm0, xmm0, 0           ; broadcast value into all four lanes
.loop16:
    cmp     A3, 16
    jb      .loop4
    movdqu  [A1], xmm0
    movdqu  [A1 + 16], xmm0
    movdqu  [A1 + 32], xmm0
    movdqu  [A1 + 48], xmm0
    add     A1, 64
    sub     A3, 16
    jmp     .loop16
.loop4:
    cmp     A3, 4
    jb      .tail
    movdqu  [A1], xmm0
    add     A1, 16
    sub     A3, 4
    jmp     .loop4
.tail:
    test    A3, A3
    jz      .done
    mov     [A1], A2D
    add     A1, 4
    dec     A3
    jmp     .tail
.done:
    ret

; void ge_fill_column_u32(uint32_t* dst, size_t stride_bytes, uint32_t value, size_t count)
;   Writes `value` to `count` pixels spaced `stride_bytes` apart, starting at dst.
GE_FUNC ge_fill_column_u32
.loop4:
    cmp     A4, 4
    jb      .tail
    mov     [A1], A3D
    add     A1, A2
    mov     [A1], A3D
    add     A1, A2
    mov     [A1], A3D
    add     A1, A2
    mov     [A1], A3D
    add     A1, A2
    sub     A4, 4
    jmp     .loop4
.tail:
    test    A4, A4
    jz      .done
    mov     [A1], A3D
    add     A1, A2
    dec     A4
    jmp     .tail
.done:
    ret

; uint64_t ge_fnv1a64_u32(const uint32_t* pixels, size_t count, uint64_t hash)
;   Continues an FNV-1a 64-bit hash over the little-endian bytes of each pixel.
GE_FUNC ge_fnv1a64_u32
    mov     rax, A3                 ; running hash (A3 is free as a scratch afterwards)
    mov     r11, 0x100000001b3      ; FNV-1a 64-bit prime
.next:
    test    A2, A2
    jz      .done
    mov     r10d, [A1]
    movzx   A3D, r10b
    xor     rax, A3
    imul    rax, r11
    shr     r10d, 8
    movzx   A3D, r10b
    xor     rax, A3
    imul    rax, r11
    shr     r10d, 8
    movzx   A3D, r10b
    xor     rax, A3
    imul    rax, r11
    shr     r10d, 8
    movzx   A3D, r10b
    xor     rax, A3
    imul    rax, r11
    add     A1, 4
    dec     A2
    jmp     .next
.done:
    ret

GE_END
