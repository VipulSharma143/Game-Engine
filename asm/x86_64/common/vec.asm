; Batch vector math, assembled on Linux (System V) and Windows (x64).
; Baseline x86-64 only (SSE2), so no CPU feature check is needed.
; The scale factor is passed by pointer so no floating-point argument register
; (which differs between the two ABIs) is involved. C++ reference:
; ge::scalar::vec2_madd in include/engine/optimized/ScalarFallback.hpp.

%include "abi.inc"

; void ge_vec2_madd(double* dst, const double* src, const double* scale, size_t count)
;   dst[i] += src[i] * (*scale) for i in [0, count). `count` is in doubles (2 per vec2).
;   Pointers may be unaligned; dst and src must not overlap. Uses xmm0-xmm2 only.
GE_FUNC ge_vec2_madd
    movsd   xmm1, [A3]
    unpcklpd xmm1, xmm1             ; scale in both lanes
.loop2:
    cmp     A4, 2
    jb      .tail
    movupd  xmm0, [A2]
    mulpd   xmm0, xmm1              ; src * scale (rounded, same as scalar)
    movupd  xmm2, [A1]
    addpd   xmm2, xmm0              ; dst + product
    movupd  [A1], xmm2
    add     A1, 16
    add     A2, 16
    sub     A4, 2
    jmp     .loop2
.tail:
    test    A4, A4
    jz      .done
    movsd   xmm0, [A2]
    mulsd   xmm0, xmm1
    addsd   xmm0, [A1]
    movsd   [A1], xmm0
.done:
    ret

GE_END
