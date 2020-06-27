    .global ol_transpose_8x8w
    .type   ol_transpose_8x8w, %function
ol_transpose_8x8w:
    vld1.16 {q0}, [r0:128]
    add r0, r0, r1
    vld1.16 {q1}, [r0:128]
    add r0, r0, r1
    vld1.16 {q2}, [r0:128]
    add r0, r0, r1
    vld1.16 {q3}, [r0:128]
    add r0, r0, r1
    vld1.16 {q8}, [r0:128]
    add r0, r0, r1
    vld1.16 {q9}, [r0:128]
    add r0, r0, r1
    vld1.16 {q10}, [r0:128]
    add r0, r0, r1
    vld1.16 {q11}, [r0:128]

    // swap antidiagonal elements within 2x2 blocks
    vtrn.16  q0, q1
    vtrn.16  q2, q3
    vtrn.16  q8, q9
    vtrn.16  q10, q11

    // swap antidiagonal 2x2 blocks within 4x4 blocks
    vtrn.32  q0, q2
    vtrn.32  q1, q3
    vtrn.32  q8, q10
    vtrn.32  q9, q11

    // swap antidiagonal 4x4 blocks within 8x8 matrix
    vswp     d1, d16
    vswp     d3, d18
    vswp     d5, d20
    vswp     d7, d22

    vst1.16 {q11}, [r0:128]
    sub r0, r0, r1
    vst1.16 {q10}, [r0:128]
    sub r0, r0, r1
    vst1.16 {q9}, [r0:128]
    sub r0, r0, r1
    vst1.16 {q8}, [r0:128]
    sub r0, r0, r1
    vst1.16 {q3}, [r0:128]
    sub r0, r0, r1
    vst1.16 {q2}, [r0:128]
    sub r0, r0, r1
    vst1.16 {q1}, [r0:128]
    sub r0, r0, r1
    vst1.16 {q0}, [r0:128]

    bx lr



    .global ol_transpose_2x8x8
    .type   ol_transpose_2x8x8, %function
ol_transpose_2x8x8:
    vld1.8 {q0}, [r0:128]
    add r0, r0, r1
    vld1.8 {q1}, [r0:128]
    add r0, r0, r1
    vld1.8 {q2}, [r0:128]
    add r0, r0, r1
    vld1.8 {q3}, [r0:128]
    add r0, r0, r1
    vld1.8 {q8}, [r0:128]
    add r0, r0, r1
    vld1.8 {q9}, [r0:128]
    add r0, r0, r1
    vld1.8 {q10}, [r0:128]
    add r0, r0, r1
    vld1.8 {q11}, [r0:128]

    vtrn.8  q0, q1
    vtrn.8  q2, q3
    vtrn.8  q8, q9
    vtrn.8  q10, q11

    vtrn.16  q0, q2
    vtrn.16  q1, q3
    vtrn.16  q8, q10
    vtrn.16  q9, q11

    vtrn.32  q0, q8
    vtrn.32  q1, q9
    vtrn.32  q2, q10
    vtrn.32  q3, q11

    vst1.8 {q11}, [r0:128]
    sub r0, r0, r1
    vst1.8 {q10}, [r0:128]
    sub r0, r0, r1
    vst1.8 {q9}, [r0:128]
    sub r0, r0, r1
    vst1.8 {q8}, [r0:128]
    sub r0, r0, r1
    vst1.8 {q3}, [r0:128]
    sub r0, r0, r1
    vst1.8 {q2}, [r0:128]
    sub r0, r0, r1
    vst1.8 {q1}, [r0:128]
    sub r0, r0, r1
    vst1.8 {q0}, [r0:128]

    bx lr

// unpack 16 bytes into 16 shorts, multiply by current kernel, accumulate
// used q15 as temp
.macro CONVOLVE qout0 qout1 qin0 din0l din0h qin1 din1l din1h dkernl dkernh
    vzip.8 \qin0, \qin1

    //same as SSE2 pmulhuw
    vmull.u16 q15, \din0l, \dkernl
    vmull.u16 \qin0, \din0h, \dkernh
    vuzp.16 q15, \qin0

    vmull.u16 q15, \din1l, \dkernl
    vmull.u16 \qin1, \din1h, \dkernh
    vuzp.16 q15, \qin1

    vqadd.u16 \qout0, \qin0
    vqadd.u16 \qout1, \qin1
.endm

// run one iteration (two adjacent convolutions to two accumulators)
.macro CONVITER
    vld1.16 {q8}, [r4:128] // load kernel
    vmov q10, q9

    CONVOLVE q0 q1 q9 d18 d19 q10 d20 d21 d16 d17

    // load src
    vld1.8 {q11}, [r0:128]
    vmov q10, q11
    vmov q9, q11

    CONVOLVE q2 q3 q10 d20 d21 q11 d22 d23 d16 d17
.endm

/*
 ******************************************************************************
 * SSE2 Gaussian Blur (convolution)
 * input: u8
 * output: u8
 * Runs a positive convoltion kernel on ksize input rows producing one output
 * row, 16 pixels at a time, storing in two 8-pixel runs 8 rows apart for
 * later 8x8 transposing.
 *****************************************************************************
 * void ol_conv_sse2(uint8_t *src, uint8_t *dst, size_t w, size_t h,
 *                   uint16_t *kern, size_t ksize);
 *****************************************************************************
 */
    .global ol_conv_sse2
    .type   ol_conv_sse2, %function
ol_conv_sse2:
    stmfd sp!,{r4-r5,lr}

    // load kern, ksize parameter
    ldr r4, [sp, #12]
    ldr r5, [sp, #16]

    // clear accumulators
    veor q0, q0
    veor q1, q1
    veor q2, q2
    veor q3, q3

    // preload first line
    vld1.8 {q9}, [r0:128]
    add    r0, r0, r2 //src += w

    // convolve loop
.loop:
    CONVITER
    add r4, r4, #16
    add r0, r0, r2
    subs  r5, r5, #1
    bne .loop

    // pack back into bytes and store as two 8-byte chunks
    vuzp.8 q1, q0
    vuzp.8 q3, q2

    add r4, r1, r3, lsl #3
    vst1.8 {d1}, [r1:64]
    vst1.8 {d0}, [r4:64]
    add r1, r1, r3
    add r4, r4, r3
    vst1.8 {d5}, [r1:64]
    vst1.8 {d4}, [r4:64]

    ldmfd sp!,{r4-r5,pc}

/*
 *****************************************************************************
 * SSE2 Sobel (hardcoded convolutions)
 * input: u8 or s16
 * output: s16
 * Runs Sobel transform on 10 input rows producing 8 output rows, 16 pixels at
 * a time (16x8 input), transposing the two 8x8 blocks into 8x16 that will
 * later be internally transposed
 *****************************************************************************
 * void ol_sobel_sse2_gx_v(uint8_t *src, int16_t *dst, size_t w, size_t h);
 * void ol_sobel_sse2_gx_h(int16_t *src, int16_t *dst, size_t w, size_t h);
 * void ol_sobel_sse2_gy_v(uint8_t *src, int16_t *dst, size_t w, size_t h);
 * void ol_sobel_sse2_gy_h(int16_t *src, int16_t *dst, size_t w, size_t h);
 *****************************************************************************
 */

// there are four variants generated from a single macro
//d: 1-byte 2-short input
//c: 0-[1, 2, 1] kernel 1-[1, 0, -1] kernel
.macro SOBEL a b c d
    .global ol_sobel_sse2_\a\()_\b\()
    .type   ol_sobel_sse2_\a\()_\b\(), %function
ol_sobel_sse2_\a\()_\b\():
    stmfd sp!,{r4,lr}
    mov r3, r3, lsl #1

    // prime m0-1 amd m2-3 with first two rows
.if \d == 1
    // u8 input, load two 16-byte rows and unpack
    vld1.8 {q0}, [r0:128]
    add r0, r0, r2
    vld1.8 {q2}, [r0:128]
    add r0, r0, r2
    veor q1, q1 // clear q1
    veor q3, q3 // clear q3
    vzip.8 q0, q1
    vzip.8 q2, q3
.else
    // s16 input, load two 2x8-short rows
    mov r2, r2, lsl #1

    vld1.16 {q0, q1}, [r0:128]
    add r0, r0, r2
    vld1.16 {q2, q3}, [r0:128]
    add r0, r0, r2
.endif

    add r4, r1, r3, lsl #3
    // do 8 rows
.rept 8
    // load the third++ row
.if \d == 1
    // u8 input, load one 16-byte rows and unpack
    vld1.8 {q8}, [r0:128]
    veor q9, q9 // clear q9
    vzip.8 q8, q9
.else
    // s16 input, load two 2x8-short rows
    vld1.16 {q8, q9}, [r0:128]
.endif
    add r0, r0, r2

    // perform the kernel operation
.if \c == 0
    // [1, 2, 1]
    // row 1 += row 2 + row 2 + row 3
    vadd.i16 q0, q2
    vadd.i16 q0, q2
    vadd.i16 q1, q3
    vadd.i16 q1, q3

    vadd.i16 q0, q8
    vadd.i16 q1, q9
.else
    // [1, 0, -1]
    // row 1 -= row 3
    vsub.i16 q0, q8
    vsub.i16 q1, q9
.endif

    // store
    vst1.16 {q0}, [r1:128]
    vst1.16 {q1}, [r4:128]
    add r1, r1, r3
    add r4, r4, r3

    // shift rows for the next iteration
    vmov q0, q2
    vmov q1, q3
    vmov q2, q8
    vmov q3, q9
.endr

    ldmfd sp!,{r4,pc}
.endm

// generate the Sobel variants
SOBEL gx, v, 0, 1
SOBEL gx, h, 1, 2
SOBEL gy, v, 1, 1
SOBEL gy, h, 0, 2
