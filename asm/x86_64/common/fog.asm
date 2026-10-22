; Distance-fog blend for rows and columns of 0xAARRGGBB pixels, Linux System V and Windows x64.
;
;   for each pixel and each of its 4 byte channels c:
;       out = (c * light + fog_c * (256 - light)) >> 8        (light in [0, 256])
;
; Exact C++ reference: ge::scalar::fog_blend (include/engine/optimized/ScalarFallback.hpp).
; Every intermediate fits in 16 bits (255*256 = 65280), so the 16-bit multiply/add/shift
; below is bit-identical to the scalar version.
;
; void ge_fog_blend_sse2(uint32_t* dst, const uint32_t* src, size_t count, const FogParams* p)
; void ge_fog_blend_avx2(uint32_t* dst, const uint32_t* src, size_t count, const FogParams* p)
;   FogParams = { uint32_t light; uint32_t fog_color; }   (passed by pointer: no float/5th-arg ABI differences)
;   dst may equal src (in-place); otherwise they must not overlap. Pointers may be unaligned.
;   Registers: only xmm0-xmm5/ymm0-ymm5 (volatile on both ABIs; xmm6-15 are callee-saved on
;   Windows and are never touched), rax, and the argument registers. AVX2 path ends with vzeroupper.
;   The AVX2 routine must only be called after a CPUID + XGETBV check (see CpuFeatures).

%include "abi.inc"

; ---- SSE2: 4 pixels per iteration -------------------------------------------------------
GE_FUNC ge_fog_blend_sse2
    movd        xmm3, [A4]              ; light
    pshuflw     xmm3, xmm3, 0
    punpcklqdq  xmm3, xmm3              ; light in all 8 words
    mov         eax, 256
    sub         eax, [A4]
    movd        xmm4, eax
    pshuflw     xmm4, xmm4, 0
    punpcklqdq  xmm4, xmm4              ; (256 - light) in all 8 words
    pxor        xmm2, xmm2
    movd        xmm5, [A4 + 4]          ; fog colour bytes
    punpcklbw   xmm5, xmm2              ; one pixel as 4 words
    punpcklqdq  xmm5, xmm5              ; two pixels
    pmullw      xmm5, xmm4              ; fog * (256 - light), reused every iteration
.loop4:
    cmp         A3, 4
    jb          .tail
    movdqu      xmm0, [A2]
    movdqa      xmm1, xmm0
    punpcklbw   xmm0, xmm2              ; pixels 0,1 -> words
    punpckhbw   xmm1, xmm2              ; pixels 2,3 -> words
    pmullw      xmm0, xmm3
    pmullw      xmm1, xmm3
    paddw       xmm0, xmm5
    paddw       xmm1, xmm5
    psrlw       xmm0, 8
    psrlw       xmm1, 8
    packuswb    xmm0, xmm1
    movdqu      [A1], xmm0
    add         A1, 16
    add         A2, 16
    sub         A3, 4
    jmp         .loop4
.tail:
    test        A3, A3
    jz          .done
    movd        xmm0, [A2]
    punpcklbw   xmm0, xmm2
    pmullw      xmm0, xmm3
    paddw       xmm0, xmm5
    psrlw       xmm0, 8
    packuswb    xmm0, xmm0
    movd        [A1], xmm0
    add         A1, 4
    add         A2, 4
    dec         A3
    jmp         .tail
.done:
    ret

; ---- AVX2: 8 pixels per iteration ---------------------------------------------------------
GE_FUNC ge_fog_blend_avx2
    vpbroadcastw ymm3, [A4]             ; light (low word of the dword) in all 16 words
    mov         eax, 256
    sub         eax, [A4]
    vmovd       xmm4, eax
    vpbroadcastw ymm4, xmm4             ; 256 - light
    vpxor       ymm2, ymm2, ymm2
    vmovd       xmm5, [A4 + 4]
    vpmovzxbw   xmm5, xmm5              ; one pixel as 4 words
    vpbroadcastq ymm5, xmm5             ; replicated for 4 pixels per 128-bit lane pair
    vpmullw     ymm5, ymm5, ymm4        ; fog * (256 - light)
.loop8:
    cmp         A3, 8
    jb          .tail
    vmovdqu     ymm0, [A2]
    vpunpcklbw  ymm1, ymm0, ymm2        ; pixels 0,1 | 4,5
    vpunpckhbw  ymm0, ymm0, ymm2        ; pixels 2,3 | 6,7
    vpmullw     ymm1, ymm1, ymm3
    vpmullw     ymm0, ymm0, ymm3
    vpaddw      ymm1, ymm1, ymm5
    vpaddw      ymm0, ymm0, ymm5
    vpsrlw      ymm1, ymm1, 8
    vpsrlw      ymm0, ymm0, 8
    vpackuswb   ymm0, ymm1, ymm0        ; in-lane pack restores pixel order 0..7
    vmovdqu     [A1], ymm0
    add         A1, 32
    add         A2, 32
    sub         A3, 8
    jmp         .loop8
.tail:
    test        A3, A3
    jz          .done
    vmovd       xmm0, [A2]
    vpunpcklbw  xmm0, xmm0, xmm2
    vpmullw     xmm0, xmm0, xmm3
    vpaddw      xmm0, xmm0, xmm5
    vpsrlw      xmm0, xmm0, 8
    vpackuswb   xmm0, xmm0, xmm0
    vmovd       [A1], xmm0
    add         A1, 4
    add         A2, 4
    dec         A3
    jmp         .tail
.done:
    vzeroupper
    ret

GE_END
