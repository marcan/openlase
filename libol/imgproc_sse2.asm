;*****************************************************************************
;*         OpenLase - a realtime laser graphics toolkit
;*
;* Copyright (C) 2009-2011 Hector Martin "marcan" <hector@marcansoft.com>
;*
;* This program is free software; you can redistribute it and/or modify
;* it under the terms of the GNU General Public License as published by
;* the Free Software Foundation, either version 2 or version 3.
;*
;* This program is distributed in the hope that it will be useful,
;* but WITHOUT ANY WARRANTY; without even the implied warranty of
;* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;* GNU General Public License for more details.
;*
;* You should have received a copy of the GNU General Public License
;* along with this program; if not, write to the Free Software
;* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
;*
;*****************************************************************************

;*****************************************************************************
;* Transpose helpers based on x264 code (x86util.asm), licensed GPLv2+ as
;* follows:
;*****************************************************************************
;* x86util.asm: x86 utility macros
;*****************************************************************************
;* Copyright (C) 2008-2011 x264 project
;*
;* Authors: Holger Lubitz <holger@lubitz.org>
;*          Loren Merritt <lorenm@u.washington.edu>
;*****************************************************************************

%include "x86inc.asm"

SECTION .text
INIT_XMM

;*****************************************************************************
;* Transpose helpers
;*****************************************************************************

%macro SBUTTERFLY 4
%if avx_enabled == 0
    mova      m%4, m%2
    punpckl%1 m%2, m%3
    punpckh%1 m%4, m%3
%else
    punpckh%1 m%4, m%2, m%3
    punpckl%1 m%2, m%3
%endif
    SWAP %3, %4
%endmacro

%macro TRANSPOSE2x8x8B 9-11
%ifdef ARCH_X86_64
    SBUTTERFLY bw,  %1, %2, %9
    SBUTTERFLY bw,  %3, %4, %9
    SBUTTERFLY bw,  %5, %6, %9
    SBUTTERFLY bw,  %7, %8, %9
    SBUTTERFLY wd,  %1, %3, %9
    SBUTTERFLY wd,  %2, %4, %9
    SBUTTERFLY wd,  %5, %7, %9
    SBUTTERFLY wd,  %6, %8, %9
    SBUTTERFLY dq,  %1, %5, %9
    SBUTTERFLY dq,  %2, %6, %9
    SBUTTERFLY dq,  %3, %7, %9
    SBUTTERFLY dq,  %4, %8, %9
    SBUTTERFLY qdq, %1, %2, %9
    SBUTTERFLY qdq, %5, %6, %9
    SBUTTERFLY qdq, %3, %4, %9
    SBUTTERFLY qdq, %7, %8, %9
    SWAP %3, %5
    SWAP %4, %6
%else
; in:  m0..m7, unless %11 in which case m6 is in %9
; out: m0..m7, unless %11 in which case m1 is in %9
; spills into %9 and %10
%if %0<11
    movdqa %9, m%7
%endif
    SBUTTERFLY bw,  %1, %2, %7
    movdqa %10, m%2
    movdqa m%7, %9
    SBUTTERFLY bw,  %3, %4, %2
    SBUTTERFLY bw,  %5, %6, %2
    SBUTTERFLY bw,  %7, %8, %2
    SBUTTERFLY wd,  %1, %3, %2
    movdqa %9, m%3
    movdqa m%2, %10
    SBUTTERFLY wd,  %2, %4, %3
    SBUTTERFLY wd,  %5, %7, %3
    SBUTTERFLY wd,  %6, %8, %3
    SBUTTERFLY dq,  %1, %5, %3
    SBUTTERFLY dq,  %2, %6, %3
    movdqa %10, m%5
    movdqa m%3, %9
    SBUTTERFLY dq,  %3, %7, %5
    SBUTTERFLY dq,  %4, %8, %5
    SBUTTERFLY qdq, %1, %2, %5
    movdqa %9, m%2
    movdqa m%5, %10
    SBUTTERFLY qdq, %5, %6, %2
    SBUTTERFLY qdq, %3, %4, %2
    SBUTTERFLY qdq, %7, %8, %2	
    SWAP %3, %5
    SWAP %4, %6
%if %0<11
    movdqa m%2, %9
%endif
%endif
%endmacro

%macro TRANSPOSE8x8W 9-11
%ifdef ARCH_X86_64
    SBUTTERFLY wd,  %1, %2, %9
    SBUTTERFLY wd,  %3, %4, %9
    SBUTTERFLY wd,  %5, %6, %9
    SBUTTERFLY wd,  %7, %8, %9
    SBUTTERFLY dq,  %1, %3, %9
    SBUTTERFLY dq,  %2, %4, %9
    SBUTTERFLY dq,  %5, %7, %9
    SBUTTERFLY dq,  %6, %8, %9
    SBUTTERFLY qdq, %1, %5, %9
    SBUTTERFLY qdq, %2, %6, %9
    SBUTTERFLY qdq, %3, %7, %9
    SBUTTERFLY qdq, %4, %8, %9
    SWAP %2, %5
    SWAP %4, %7
%else
; in:  m0..m7, unless %11 in which case m6 is in %9
; out: m0..m7, unless %11 in which case m4 is in %10
; spills into %9 and %10
%if %0<11
    movdqa %9, m%7
%endif
    SBUTTERFLY wd,  %1, %2, %7
    movdqa %10, m%2
    movdqa m%7, %9
    SBUTTERFLY wd,  %3, %4, %2
    SBUTTERFLY wd,  %5, %6, %2
    SBUTTERFLY wd,  %7, %8, %2
    SBUTTERFLY dq,  %1, %3, %2
    movdqa %9, m%3
    movdqa m%2, %10
    SBUTTERFLY dq,  %2, %4, %3
    SBUTTERFLY dq,  %5, %7, %3
    SBUTTERFLY dq,  %6, %8, %3
    SBUTTERFLY qdq, %1, %5, %3
    SBUTTERFLY qdq, %2, %6, %3
    movdqa %10, m%2
    movdqa m%3, %9
    SBUTTERFLY qdq, %3, %7, %2
    SBUTTERFLY qdq, %4, %8, %2
    SWAP %2, %5
    SWAP %4, %7
%if %0<11
    movdqa m%5, %10
%endif
%endif
%endmacro

;*****************************************************************************
;* SSE2 Gaussian Blur (convolution)
;* input: u8
;* output: u8
;* Runs a positive convoltion kernel on ksize input rows producing one output
;* row, 16 pixels at a time, storing in two 8-pixel runs 8 rows apart for
;* later 8x8 transposing.
;*****************************************************************************
;* void ol_conv_sse2(uint8_t *src, uint8_t *dst, size_t w, size_t h,
;*                   uint16_t *kern, size_t ksize);
;*****************************************************************************

; unpack 16 bytes into 16 shorts, multiply by current kernel, accumulate
%macro CONVOLVE 5
    punpcklbw %3, %3
    punpckhbw %4, %4
    pmulhuw %3, %5
    pmulhuw %4, %5
    paddw   %1, %3
    paddw   %2, %4
%endmacro

; run one iteration (two adjacent convolutions to two accumulators)
%macro CONVITER 2
    mova    m4, %1

    mova    m6, m5
    CONVOLVE m0, m1, m5, m6, m4

    mova    m7, %2
    mova    m6, m7
    mova    m5, m7

    CONVOLVE m2, m3, m6, m7, m4
%endmacro

cglobal conv_sse2, 6,6,8
    ; clear accumulators
    pxor    m0, m0
    pxor    m1, m1
    pxor    m2, m2
    pxor    m3, m3

    ; preload first line
    mova    m5, [r0]
    add     r0, r2

    ; convolve against the first kernel row
    CONVITER [r4], [r0]
    dec     r5d

.loop

    ; now convolve two kernel rows at a time
    CONVITER [r4+16], [r0+r2]
    CONVITER [r4+32], [r0+2*r2]

    add     r4, 32
    lea     r0, [r0+2*r2]

    sub     r5d, 2
    jg      .loop

    ; shift back into bytes, pack and store as two 8-byte chunks
    psrlw   m0, 8
    psrlw   m1, 8
    psrlw   m2, 8
    psrlw   m3, 8

    lea     r2, [r1+r3]
    packuswb m0, m1
    packuswb m2, m3
    movq    [r1], m0
    movhps  [r1+8*r3], m0
    movq    [r2], m2
    movhps  [r2+8*r3], m2

    RET

;*****************************************************************************
;* SSE2 Sobel (hardcoded convolutions)
;* input: u8 or s16
;* output: s16
;* Runs Sobel transform on 10 input rows producing 8 output rows, 16 pixels at
;* a time (16x8 input), transposing the two 8x8 blocks into 8x16 that will
;* later be internally transposed
;*****************************************************************************
;* void ol_sobel_sse2_gx_v(uint8_t *src, int16_t *dst, size_t w, size_t h);
;* void ol_sobel_sse2_gx_h(int16_t *src, int16_t *dst, size_t w, size_t h);
;* void ol_sobel_sse2_gy_v(uint8_t *src, int16_t *dst, size_t w, size_t h);
;* void ol_sobel_sse2_gy_h(int16_t *src, int16_t *dst, size_t w, size_t h);
;*****************************************************************************

; there are four variants generated from a single macro
%macro SOBEL 4
cglobal sobel_sse2_%1_%2, 4,4,7
    shl     r3, 1

    ; prime m0-1 amd m2-3 with first two rows
%if %4 == 1
    ; u8 input, load two 16-byte rows and unpack
    pxor    m6, m6
    mova    m0, [r0]
    mova    m2, [r0+r2]
    mova    m1, m0
    mova    m3, m2
    punpcklbw m0, m6
    punpckhbw m1, m6
    punpcklbw m2, m6
    punpckhbw m3, m6
%elif %4 == 2
    ; s16 input, load two 2x8-short rows
    shl     r2, 1
    mova    m0, [r0]
    mova    m1, [r0+16]
    mova    m2, [r0+r2]
    mova    m3, [r0+r2+16]
%endif

    ; do 8 rows
%rep 8
    ; load the third row
    mova    m4, [r0+2*r2]
%if %4 == 1
    mova    m5, m4
    punpcklbw m4, m6
    punpckhbw m5, m6
%elif %4 == 2
    mova    m5, [r0+2*r2+16]
%endif
    add     r0, r2

    ; perform the kernel operation
%if %3 == 0
    ; [1, 2, 1]
    paddw   m0, m2
    paddw   m1, m3
    paddw   m0, m2
    paddw   m1, m3
    paddw   m0, m4
    paddw   m1, m5
%elif %3 == 1
    ; [1, 0, -1]
    psubw   m0, m4
    psubw   m1, m5
%endif

    ; store
    mova    [r1], m0
    mova    [r1+8*r3], m1
    add     r1, r3

    ; shift rows for the next iteration
    SWAP    m0, m2
    SWAP    m1, m3
    SWAP    m2, m4
    SWAP    m3, m5
%endrep

    RET
%endmacro

; generate the Sobel variants
SOBEL gx, v, 0, 1
SOBEL gx, h, 1, 2
SOBEL gy, v, 1, 1
SOBEL gy, h, 0, 2

;*****************************************************************************
;* SSE2 2x8x8 Transpose
;* input: u8
;* output: u8
;* Transposes 16x8 pixels as two adjacent 8x8 blocks simultaneously
;*****************************************************************************
;* void ol_transpose_2x8x8(u8 *p);
;*****************************************************************************

cglobal transpose_2x8x8, 2,5,9
%ifdef ARCH_X86_64
    %define t0 8
    %define t1 9
%else
    %define t0 [rsp-mmsize]
    %define t1 [rsp-2*mmsize]
    mov     r4, esp
    and     esp, ~15
%endif

    lea     r2, [r0+r1]
    lea     r3, [r1+2*r1]
    mova    m0, [r0]
    mova    m1, [r2]
    mova    m2, [r0+2*r1]
    mova    m3, [r2+2*r1]
    mova    m4, [r0+4*r1]
    mova    m5, [r2+4*r1]
    mova    m6, [r0+2*r3]
    mova    m7, [r2+2*r3]

    TRANSPOSE2x8x8B 0, 1, 2, 3, 4, 5, 6, 7, t0, t1

    mova    [r0], m0
    mova    [r2], m1
    mova    [r0+2*r1], m2
    mova    [r2+2*r1], m3
    mova    [r0+4*r1], m4
    mova    [r2+4*r1], m5
    mova    [r0+2*r3], m6
    mova    [r2+2*r3], m7

%ifndef ARCH_X86_64
    mov     esp, r4
%endif
    RET

;*****************************************************************************
;* SSE2 8x8 Transpose
;* input: s16
;* output: s16
;* Transposes 8x8 shorts
;*****************************************************************************
;* void ol_transpose_8x8w(u16 *p);
;*****************************************************************************

cglobal transpose_8x8w, 2,5,9
%ifdef ARCH_X86_64
    %define t0 8
    %define t1 9
%else
    %define t0 [rsp-mmsize]
    %define t1 [rsp-2*mmsize]
    mov     r4, esp
    and     esp, ~15
%endif

    lea     r2, [r0+r1]
    lea     r3, [r1+2*r1]
    mova    m0, [r0]
    mova    m1, [r2]
    mova    m2, [r0+2*r1]
    mova    m3, [r2+2*r1]
    mova    m4, [r0+4*r1]
    mova    m5, [r2+4*r1]
    mova    m6, [r0+2*r3]
    mova    m7, [r2+2*r3]

    TRANSPOSE8x8W 0, 1, 2, 3, 4, 5, 6, 7, t0, t1

    mova    [r0], m0
    mova    [r2], m1
    mova    [r0+2*r1], m2
    mova    [r2+2*r1], m3
    mova    [r0+4*r1], m4
    mova    [r2+4*r1], m5
    mova    [r0+2*r3], m6
    mova    [r2+2*r3], m7

%ifndef ARCH_X86_64
    mov     esp, r4
%endif
    RET
