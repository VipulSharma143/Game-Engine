; Span kernels for the textured renderer and the sprite renderer, Linux System V and Windows x64.
;
;   ge_floor_span_{sse2,avx2}   floor / ceiling texture sampling for one screen row
;   ge_sprite_span_{sse2,avx2}  gather + alpha test + shade of one sprite row (returns non-zero if any pixel was written)
;
; Exact C++ references: ge::scalar::floor_span and ge::scalar::sprite_span
; (include/engine/optimized/ScalarFallback.hpp). Results are bit-identical; tests/assembly checks it.
;
; Parameters are passed as one aligned block (FloorSpanParams / SpriteSpanParams in
; include/engine/optimized/OptimizedRoutines.hpp) so the register-argument difference between the two
; ABIs does not matter and no double or fifth argument is needed. The block must be 32-byte aligned.
;
; Register use: xmm0-xmm5 / ymm0-ymm5 (volatile on both ABIs; xmm6-15 are never touched), rax, r10, r11
; and the argument registers. No stack is used. The AVX2 routines end with vzeroupper and must only be
; called after the CPUID + XGETBV check (see CpuFeatures). No floating-point FMA is used anywhere: the
; C++ reference does a separate multiply and add, and the results must match bit for bit.

%include "abi.inc"

; FloorSpanParams layout
%define FS_FX   0       ; double[2]  position of pixel 0 (both lanes equal)
%define FS_FY   16
%define FS_SX   32      ; double[2]  step per pixel
%define FS_SY   48
%define FS_PM   64      ; uint32[4]  0 or 1: 1 = pick the texture by (cx + cy) & 1 (floor checkerboard)
%define FS_TEX  80      ; const uint32_t*  texels: 64x64 texture, or two of them back to back

; SpriteSpanParams layout
%define SP_L    0       ; uint16[16] light  (8.8 fixed point, 0..1023) in every word
%define SP_F    32      ; uint16[16] flash  (0..256) in every word
%define SP_ROW  64      ; const uint32_t*  texels of this image row
%define SP_IX   72      ; const int32_t*   texel column per output pixel, negative = leave the pixel alone

GE_RODATA
align 32
ge_k_one_pd:    dq 1.0, 1.0, 1.0, 1.0
ge_k_64_pd:     dq 64.0, 64.0, 64.0, 64.0
ge_k_two_pd:    dq 2.0, 2.0, 2.0, 2.0
ge_k_four_pd:   dq 4.0, 4.0, 4.0, 4.0
ge_k_eight_pd:  dq 8.0, 8.0, 8.0, 8.0
ge_k_iv0_sse:   dq 0.0, 1.0, 0.0, 1.0
ge_k_iv0_avx:   dq 0.0, 1.0, 2.0, 3.0
ge_k_63_d:      times 8 dd 63
ge_k_m1_d:      times 8 dd -1
ge_k_lane_ids:  dd 0, 1, 2, 3, 4, 5, 6, 7
ge_k_255_w:     times 16 dw 255
ge_k_alpha_d:   times 8 dd 0xFF000000

; ======================================================================================
; Floor / ceiling row
;   dst[i] = texels[ ((cx + cy) & pm) << 12 | (ty & 63) << 6 | (tx & 63) ]  with
;   x = fx + i*sx, y = fy + i*sy, cx = floor(x), tx = int((x - cx) * 64), same for y.
;   |x|, |y| must be below 2^30 (the int32 conversion).
; void ge_floor_span_sse2(uint32_t* dst, size_t count, const FloorSpanParams* p)
; ======================================================================================

; In: xmm4 = [i, i+1] as doubles.  Out: xmm0 low two dwords = texel indices.  Clobbers xmm0-xmm3, xmm5.
%macro FS_PAIR_SSE2 0
    movapd      xmm0, xmm4
    mulpd       xmm0, [A3 + FS_SX]
    addpd       xmm0, [A3 + FS_FX]          ; x
    cvttpd2dq   xmm1, xmm0                  ; truncate ...
    cvtdq2pd    xmm2, xmm1
    movapd      xmm3, xmm0
    cmpltpd     xmm3, xmm2                  ; ... and subtract 1 where x < truncated (negative, non-integer)
    andpd       xmm3, [ge_k_one_pd]
    subpd       xmm2, xmm3                  ; floor(x)
    subpd       xmm0, xmm2                  ; fraction in [0, 1]
    mulpd       xmm0, [ge_k_64_pd]
    cvttpd2dq   xmm0, xmm0                  ; tx
    cvttpd2dq   xmm1, xmm2                  ; cx
    pand        xmm1, [A3 + FS_PM]
    pslld       xmm1, 12
    pand        xmm0, [ge_k_63_d]
    por         xmm0, xmm1
    movdqa      xmm5, xmm0                  ; x part of the index

    movapd      xmm0, xmm4
    mulpd       xmm0, [A3 + FS_SY]
    addpd       xmm0, [A3 + FS_FY]          ; y
    cvttpd2dq   xmm1, xmm0
    cvtdq2pd    xmm2, xmm1
    movapd      xmm3, xmm0
    cmpltpd     xmm3, xmm2
    andpd       xmm3, [ge_k_one_pd]
    subpd       xmm2, xmm3
    subpd       xmm0, xmm2
    mulpd       xmm0, [ge_k_64_pd]
    cvttpd2dq   xmm0, xmm0                  ; ty
    cvttpd2dq   xmm1, xmm2                  ; cy
    pand        xmm1, [A3 + FS_PM]
    pslld       xmm1, 12
    pand        xmm0, [ge_k_63_d]
    pslld       xmm0, 6
    por         xmm0, xmm1
    pxor        xmm0, xmm5                  ; bit 12 = (cx ^ cy) & pm, bits 6-11 = ty, bits 0-5 = tx
%endmacro

GE_FUNC ge_floor_span_sse2
    mov         A4, [A3 + FS_TEX]
    movapd      xmm4, [ge_k_iv0_sse]
.loop2:
    cmp         A2, 2
    jb          .tail
    FS_PAIR_SSE2
    movd        eax, xmm0
    pshufd      xmm0, xmm0, 1
    movd        r10d, xmm0
    mov         eax, [A4 + rax*4]
    mov         r10d, [A4 + r10*4]
    mov         [A1], eax
    mov         [A1 + 4], r10d
    add         A1, 8
    addpd       xmm4, [ge_k_two_pd]
    sub         A2, 2
    jmp         .loop2
.tail:
    test        A2, A2
    jz          .done
    FS_PAIR_SSE2
    movd        eax, xmm0
    mov         eax, [A4 + rax*4]
    mov         [A1], eax
.done:
    ret

; In: ymm3 = [i .. i+3] as doubles.  Out: xmm0 = four texel indices.  Clobbers ymm0-ymm2.
%macro FS_QUAD_AVX2 0
    vbroadcastsd ymm1, [A3 + FS_SX]
    vmulpd      ymm0, ymm3, ymm1
    vbroadcastsd ymm1, [A3 + FS_FX]
    vaddpd      ymm0, ymm0, ymm1            ; x
    vroundpd    ymm1, ymm0, 9               ; floor (round down, no exceptions)
    vsubpd      ymm0, ymm0, ymm1
    vmulpd      ymm0, ymm0, [ge_k_64_pd]
    vcvttpd2dq  xmm0, ymm0                  ; tx
    vcvttpd2dq  xmm1, ymm1                  ; cx
    vpand       xmm1, xmm1, [A3 + FS_PM]
    vpslld      xmm1, xmm1, 12
    vpand       xmm0, xmm0, [ge_k_63_d]
    vpor        xmm0, xmm0, xmm1            ; x part

    vbroadcastsd ymm1, [A3 + FS_SY]
    vmulpd      ymm2, ymm3, ymm1
    vbroadcastsd ymm1, [A3 + FS_FY]
    vaddpd      ymm2, ymm2, ymm1            ; y
    vroundpd    ymm1, ymm2, 9
    vsubpd      ymm2, ymm2, ymm1
    vmulpd      ymm2, ymm2, [ge_k_64_pd]
    vcvttpd2dq  xmm2, ymm2                  ; ty
    vcvttpd2dq  xmm1, ymm1                  ; cy
    vpand       xmm1, xmm1, [A3 + FS_PM]
    vpslld      xmm1, xmm1, 12
    vpand       xmm2, xmm2, [ge_k_63_d]
    vpslld      xmm2, xmm2, 6
    vpor        xmm2, xmm2, xmm1
    vpxor       xmm0, xmm0, xmm2
%endmacro

; Eight indices into ymm4: pixels i..i+3 from the iv in ymm5, i+4..i+7 from ymm5 + 4.
%macro FS_OCT_AVX2 0
    vmovapd     ymm3, ymm5
    FS_QUAD_AVX2
    vmovdqa     xmm4, xmm0
    vaddpd      ymm3, ymm5, [ge_k_four_pd]
    FS_QUAD_AVX2
    vinserti128 ymm4, ymm4, xmm0, 1
%endmacro

; void ge_floor_span_avx2(uint32_t* dst, size_t count, const FloorSpanParams* p)
GE_FUNC ge_floor_span_avx2
    mov         A4, [A3 + FS_TEX]
    vmovapd     ymm5, [ge_k_iv0_avx]
.loop8:
    cmp         A2, 8
    jb          .tail
    FS_OCT_AVX2
    vpcmpeqd    ymm3, ymm3, ymm3            ; gather mask: all lanes
    vpgatherdd  ymm0, [A4 + ymm4*4], ymm3
    vmovdqu     [A1], ymm0
    add         A1, 32
    vaddpd      ymm5, ymm5, [ge_k_eight_pd]
    sub         A2, 8
    jmp         .loop8
.tail:
    test        A2, A2
    jz          .done
    FS_OCT_AVX2
    vmovd       xmm1, A2D
    vpbroadcastd ymm1, xmm1
    vpcmpgtd    ymm1, ymm1, [ge_k_lane_ids] ; lane < count
    vmovdqa     ymm3, ymm1
    vpgatherdd  ymm0, [A4 + ymm4*4], ymm3   ; only the wanted lanes are read
    vpmaskmovd  [A1], ymm1, ymm0            ; only the wanted lanes are written
.done:
    vzeroupper
    ret

; ======================================================================================
; Sprite row
;   for each i with ix[i] >= 0 and alpha(row[ix[i]]) >= 128:
;       dst[i] = 0xFF000000 | shade(row[ix[i]])   with, per colour channel c,
;       v = min(255, (c * light) >> 8);  v += ((255 - v) * flash) >> 8
;   Other pixels of dst are left untouched. Returns non-zero if any pixel was written.
; uint32_t ge_sprite_span_sse2(uint32_t* dst, size_t count, const SpriteSpanParams* p)
; ======================================================================================

; Shades the four words-per-pixel values in register %1 (colour in the high byte of each word).
; Temp: xmm4. xmm5 holds the light words.
%macro SP_SHADE_SSE2 1
    pmulhuw     %1, xmm5                    ; (c << 8) * light >> 16 == (c * light) >> 8
    pminsw      %1, [ge_k_255_w]
    movdqa      xmm4, %1
    pxor        xmm4, [ge_k_255_w]          ; 255 - v for v in 0..255
    pmullw      xmm4, [A3 + SP_F]
    psrlw       xmm4, 8
    paddw       %1, xmm4
%endmacro

GE_FUNC ge_sprite_span_sse2
    mov         A4, [A3 + SP_ROW]
    mov         r10, [A3 + SP_IX]
    xor         r11d, r11d
    movdqa      xmm5, [A3 + SP_L]
.loop4:
    cmp         A2, 4
    jb          .tail
    movdqu      xmm0, [r10]
    pcmpeqd     xmm2, xmm2
    movdqa      xmm1, xmm0
    pcmpgtd     xmm1, xmm2                  ; valid: ix > -1
    pand        xmm0, xmm1                  ; invalid lanes read texel 0 (always inside the row)
    movd        eax, xmm0
    pshufd      xmm0, xmm0, 0x39
    movd        xmm2, [A4 + rax*4]
    movd        eax, xmm0
    pshufd      xmm0, xmm0, 0x39
    movd        xmm3, [A4 + rax*4]
    punpckldq   xmm2, xmm3                  ; t0 t1
    movd        eax, xmm0
    pshufd      xmm0, xmm0, 0x39
    movd        xmm3, [A4 + rax*4]
    movd        eax, xmm0
    movd        xmm0, [A4 + rax*4]
    punpckldq   xmm3, xmm0                  ; t2 t3
    punpcklqdq  xmm2, xmm3                  ; t0 t1 t2 t3
    movdqa      xmm0, xmm2
    psrad       xmm0, 31                    ; alpha >= 128
    pand        xmm1, xmm0                  ; final mask
    movmskps    eax, xmm1
    or          r11d, eax
    pxor        xmm3, xmm3
    punpcklbw   xmm3, xmm2                  ; pixels 0,1: texel byte in the high byte of each word
    pxor        xmm0, xmm0
    punpckhbw   xmm0, xmm2                  ; pixels 2,3
    SP_SHADE_SSE2 xmm3
    SP_SHADE_SSE2 xmm0
    packuswb    xmm3, xmm0
    por         xmm3, [ge_k_alpha_d]
    movdqu      xmm0, [A1]
    pand        xmm3, xmm1
    pandn       xmm1, xmm0
    por         xmm3, xmm1
    movdqu      [A1], xmm3
    add         A1, 16
    add         r10, 16
    sub         A2, 4
    jmp         .loop4
.tail:
    test        A2, A2
    jz          .done
    mov         eax, [r10]
    test        eax, eax
    js          .skip
    mov         eax, [A4 + rax*4]
    test        eax, eax
    jns         .skip                       ; alpha < 128
    pxor        xmm3, xmm3
    movd        xmm2, eax
    punpcklbw   xmm3, xmm2
    SP_SHADE_SSE2 xmm3
    packuswb    xmm3, xmm3
    movd        eax, xmm3
    or          eax, 0xFF000000
    mov         [A1], eax
    or          r11d, 1
.skip:
    add         A1, 4
    add         r10, 4
    dec         A2
    jmp         .tail
.done:
    mov         eax, r11d
    ret

; Shades the words in %1 (colour in the high byte of each word). Temp: ymm3.
%macro SP_SHADE_AVX2 1
    vpmulhuw    %1, %1, [A3 + SP_L]
    vpminsw     %1, %1, [ge_k_255_w]
    vpxor       ymm3, %1, [ge_k_255_w]
    vpmullw     ymm3, ymm3, [A3 + SP_F]
    vpsrlw      ymm3, ymm3, 8
    vpaddw      %1, %1, ymm3
%endmacro

; In: ymm1 = ix, ymm2 = valid lanes.  Gathers, tests alpha, shades, stores the wanted lanes.
%macro SP_BODY_AVX2 0
    vmovdqa     ymm3, ymm2
    vpxor       ymm0, ymm0, ymm0
    vpgatherdd  ymm0, [A4 + ymm1*4], ymm3   ; invalid lanes are not read and stay 0
    vpsrad      ymm1, ymm0, 31              ; alpha >= 128
    vpand       ymm2, ymm2, ymm1            ; final mask
    vmovmskps   eax, ymm2
    or          r11d, eax
    vpxor       ymm3, ymm3, ymm3
    vpunpcklbw  ymm1, ymm3, ymm0            ; pixels 0,1 | 4,5
    vpunpckhbw  ymm0, ymm3, ymm0            ; pixels 2,3 | 6,7
    SP_SHADE_AVX2 ymm1
    SP_SHADE_AVX2 ymm0
    vpackuswb   ymm0, ymm1, ymm0            ; in-lane pack restores pixel order 0..7
    vpor        ymm0, ymm0, [ge_k_alpha_d]
    vpmaskmovd  [A1], ymm2, ymm0
%endmacro

; uint32_t ge_sprite_span_avx2(uint32_t* dst, size_t count, const SpriteSpanParams* p)
GE_FUNC ge_sprite_span_avx2
    mov         A4, [A3 + SP_ROW]
    mov         r10, [A3 + SP_IX]
    xor         r11d, r11d
.loop8:
    cmp         A2, 8
    jb          .tail
    vmovdqu     ymm1, [r10]
    vpcmpgtd    ymm2, ymm1, [ge_k_m1_d]
    SP_BODY_AVX2
    add         A1, 32
    add         r10, 32
    sub         A2, 8
    jmp         .loop8
.tail:
    test        A2, A2
    jz          .done
    vmovd       xmm1, A2D
    vpbroadcastd ymm1, xmm1
    vpcmpgtd    ymm4, ymm1, [ge_k_lane_ids] ; lane < count
    vpmaskmovd  ymm1, ymm4, [r10]           ; the rest reads as 0 and is masked off below
    vpcmpgtd    ymm2, ymm1, [ge_k_m1_d]
    vpand       ymm2, ymm2, ymm4
    SP_BODY_AVX2
.done:
    vzeroupper
    mov         eax, r11d
    ret

GE_END
