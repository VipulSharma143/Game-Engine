# Assembly ABI

NASM routines are optional. With `GE_ASM=OFF` or NASM absent, `ge::scalar::*` is used and everything still builds.

## Routines (baseline x86-64 unless stated; the AVX2 fog kernel needs a CPU feature check)

| C++ API | Linux symbol (`common/math.asm`, `linux/math.asm`) | Windows symbol (`windows/math.asm`) |
| --- | --- | --- |
| `opt::add_i64(a, b)` | `ge_add_i64` | `ge_windows_add_i64` |
| `opt::multiply_i64(a, b)` | `ge_linux_multiply_i64` | `ge_windows_multiply_i64` |

Both wrap on overflow (two's complement), exactly like `scalar::add_i64` / `scalar::multiply_i64`.

### Pixel routines (shared source for both ABIs)

`asm/x86_64/common/pixels.asm` is assembled on Linux **and** Windows. It includes `abi.inc`, which maps the argument registers by NASM output format (`elf64` -> rdi, rsi, rdx, rcx; `win64` -> rcx, rdx, r8, r9) and provides `GE_FUNC` / `GE_END` (symbol export, `.note.GNU-stack` on ELF). Symbols are identical on both platforms.

| C++ API (`ge::opt::`) | Symbol | Contract |
| --- | --- | --- |
| `fill_u32(dst, value, count)` | `ge_fill_u32` | writes `value` to `dst[0..count)`; unaligned `dst` is fine; SSE2 `movdqu`, 16-pixel unrolled loop, scalar tail |
| `fill_column_u32(dst, stride, value, count)` | `ge_fill_column_u32` | writes `count` pixels spaced `stride` elements apart (the C++ wrapper passes bytes to the asm) |
| `fnv1a64_u32(pixels, count, hash)` | `ge_fnv1a64_u32` | continues an FNV-1a 64 hash over each pixel's 4 little-endian bytes |

They use only rax, r10, r11, the argument registers and xmm0 (all volatile on both ABIs), no stack, no calls. SSE2 is part of the x86-64 baseline, so no CPU feature check is needed. The `ge::opt::` wrappers are header-inline so a call goes directly to the NASM routine.

### Vector and fog routines (shared source for both ABIs)

| C++ API (`ge::opt::`) | Symbol | Contract |
| --- | --- | --- |
| `vec2_madd(dst, src, scale, count)` | `ge_vec2_madd` (`common/vec.asm`) | `dst[i] += src[i] * scale` over `count` doubles, SSE2, scale passed by pointer; matches the C++ loop bit for bit (separate multiply and add, no FMA) |
| `fog_blend(dst, src, count, light, fog)` | `ge_fog_blend_sse2`, `ge_fog_blend_avx2` (`common/fog.asm`) | `(c * light + fog_c * (256 - light)) >> 8` per byte channel, 4 or 8 pixels per iteration, scalar-width tail; `light` in [0, 256]; `dst` may equal `src`; unaligned pointers are fine |

`fog_blend` takes its last two arguments through a small `FogParams { light, fog_color }` struct passed by pointer, so there is no floating-point or fifth-argument register difference between the two ABIs. The AVX2 routine is selected only when `ge::cpu_features().avx2` is true, which requires the CPUID AVX2 bit **and** OSXSAVE with XGETBV reporting XMM+YMM state enabled. It ends with `vzeroupper`. Both routines use only xmm0-xmm5 / ymm0-ymm5 (xmm6-xmm15 are callee-saved on Windows and are never touched), rax and the argument registers, no stack and no calls, so stack alignment is irrelevant. `ge::opt::set_fog_kernel` forces a kernel for tests and benchmarks and refuses one the machine cannot run.

## Calling conventions

| | Linux (System V AMD64) | Windows x64 |
| --- | --- | --- |
| Integer args | rdi, rsi, rdx, rcx, r8, r9 | rcx, rdx, r8, r9 |
| Return | rax | rax |
| Shadow space | none | caller reserves 32 bytes |
| Callee-saved | rbx, rbp, r12-r15 | rbx, rbp, rdi, rsi, rsp, r12-r15 (+ xmm6-15) |
| Object format | `elf64` | `win64` |

All routines use only argument registers, rax, r10/r11 and xmm0, so they preserve all callee-saved state and need no stack frame. `common/math.asm` uses System V registers despite its name, so it is assembled on Linux only; Windows builds assemble `windows/math.asm`. `common/pixels.asm` is ABI-neutral (via `abi.inc`) and is assembled on both. Symbols are undecorated on both targets (x64).

## Rules for new routines

1. Add the NASM routine per platform (or a platform-neutral one without register assumptions).
2. Add a C++ reference in `ScalarFallback` and a dispatch function in `OptimizedRoutines`.
3. Add equivalence tests (edge values + pseudo-random) in `tests/assembly`.
4. Check CPU features before using SSE4/AVX or newer instructions (`ge::cpu_features()`, then dispatch through a kernel enum as `fog_blend` does).
5. Benchmark it; keep it only if it is measurably better (see PERFORMANCE.md).

Verification status:

- Linux: `fog.asm` was also mutation-tested: an AVX2 shift by 7, a wrong SSE2 tail step and swapped AVX2 pack operands were each caught by the tests. Linux: assembled, linked, and covered by `tests/assembly` (bit-exact against the C++ references, guard-word overrun checks, every count 0-70 and unaligned starts, a mutation check that an injected off-by-one is caught).
- Windows: `pixels.asm` and `windows/math.asm` assemble with `nasm -f win64`; disassembly shows the Windows argument registers, undecorated symbols and no callee-saved registers touched. They have **not been run or linked on Windows**.

## Adding a routine with the shared ABI macros

Write it in `common/*.asm` with `%include "abi.inc"` and `GE_FUNC name`, use only `A1..A4` (and `A1D..A4D`), `rax`, `r10`, `r11`, `xmm0-5`; export with `GE_END` at the end of the file; add the symbol to `CMakeLists` via `ge_link_assembly` in `cmake/AssemblyConfig.cmake` and the checklist above.
