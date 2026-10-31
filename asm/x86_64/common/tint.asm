; Full-screen colour tint for rows of 0xAARRGGBB pixels, Linux System V and Windows x64.
;
;   for each pixel and each of its 4 byte channels c (b, g, r, a):   out = min(255, ((c * mul) >> 8) + add)
;   mul in [0, 256] (256 = unchanged), add in [0, 255]
;
; Exact C++ reference: ge::scalar::tint_u32. Done in place.
;
; void ge_tint_sse2(uint32_t* px, size_t count, const TintParams* p)
; void ge_tint_avx2(uint32_t* px, size_t count, const TintParams* p)
;   TintParams = { uint16_t mul[16]; uint16_t add[16]; }  32-byte aligned; the 16 words repeat b, g, r, a.
;   (c << 8) * mul >> 16 equals (c * mul) >> 8 exactly; the sum is at most 255 + 255, so packuswb saturates it.
;   Registers: xmm0-xmm5 / ymm0-ymm5, the argument registers; no stack. The AVX2 routine ends with vzeroupper.

%include "abi.inc"

GE_FUNC ge_tint_sse2
    movdqa      xmm4, [A3]
    movdqa      xmm5, [A3 + 32]
.loop4:
    cmp         A2, 4
    jb          .tail
    movdqu      xmm0, [A1]
    pxor        xmm2, xmm2
    punpcklbw   xmm2, xmm0                  ; pixels 0,1 with the byte in the high half of each word
    pxor        xmm1, xmm1
    punpckhbw   xmm1, xmm0                  ; pixels 2,3
    pmulhuw     xmm2, xmm4
    pmulhuw     xmm1, xmm4
    paddw       xmm2, xmm5
    paddw       xmm1, xmm5
    packuswb    xmm2, xmm1
    movdqu      [A1], xmm2
    add         A1, 16
    sub         A2, 4
    jmp         .loop4
.tail:
    test        A2, A2
    jz          .done
    movd        xmm0, [A1]
    pxor        xmm2, xmm2
    punpcklbw   xmm2, xmm0
    pmulhuw     xmm2, xmm4
    paddw       xmm2, xmm5
    packuswb    xmm2, xmm2
    movd        [A1], xmm2
    add         A1, 4
    dec         A2
    jmp         .tail
.done:
    ret

GE_FUNC ge_tint_avx2
    vmovdqa     ymm4, [A3]
    vmovdqa     ymm5, [A3 + 32]
.loop8:
    cmp         A2, 8
    jb          .tail
    vmovdqu     ymm0, [A1]
    vpxor       ymm3, ymm3, ymm3
    vpunpcklbw  ymm1, ymm3, ymm0            ; pixels 0,1 | 4,5
    vpunpckhbw  ymm0, ymm3, ymm0            ; pixels 2,3 | 6,7
    vpmulhuw    ymm1, ymm1, ymm4
    vpmulhuw    ymm0, ymm0, ymm4
    vpaddw      ymm1, ymm1, ymm5
    vpaddw      ymm0, ymm0, ymm5
    vpackuswb   ymm0, ymm1, ymm0
    vmovdqu     [A1], ymm0
    add         A1, 32
    sub         A2, 8
    jmp         .loop8
.tail:
    test        A2, A2
    jz          .done
    vmovd       xmm0, [A1]
    vpxor       xmm3, xmm3, xmm3
    vpunpcklbw  xmm1, xmm3, xmm0
    vpmulhuw    xmm1, xmm1, xmm4
    vpaddw      xmm1, xmm1, xmm5
    vpackuswb   xmm1, xmm1, xmm1
    vmovd       [A1], xmm1
    add         A1, 4
    dec         A2
    jmp         .tail
.done:
    vzeroupper
    ret

GE_END
