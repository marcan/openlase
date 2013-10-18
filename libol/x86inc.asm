;*****************************************************************************
;* x86inc.asm: x264asm abstraction layer
;*****************************************************************************
;* Copyright (C) 2005-2011 x264 project
;*
;* Authors: Loren Merritt <lorenm@u.washington.edu>
;*          Anton Mitrofanov <BugMaster@narod.ru>
;*          Jason Garrett-Glaser <darkshikari@gmail.com>
;*
;* Permission to use, copy, modify, and/or distribute this software for any
;* purpose with or without fee is hereby granted, provided that the above
;* copyright notice and this permission notice appear in all copies.
;*
;* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
;* WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
;* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
;* ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
;* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
;* ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
;* OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
;*****************************************************************************

; This is a header file for the x264ASM assembly language, which uses
; NASM/YASM syntax combined with a large number of macros to provide easy
; abstraction between different calling conventions (x86_32, win64, linux64).
; It also has various other useful features to simplify writing the kind of
; DSP functions that are most often used in x264.

; Unlike the rest of x264, this file is available under an ISC license, as it
; has significant usefulness outside of x264 and we want it to be available
; to the largest audience possible.  Of course, if you modify it for your own
; purposes to add a new feature, we strongly encourage contributing a patch
; as this feature might be useful for others as well.  Send patches or ideas
; to x264-devel@videolan.org .

%define program_name ol

%ifdef ARCH_X86_64
    %ifidn __OUTPUT_FORMAT__,win32
        %define WIN64
    %else
        %define UNIX64
    %endif
%endif

%ifdef PREFIX
    %define mangle(x) _ %+ x
%else
    %define mangle(x) x
%endif

; FIXME: All of the 64bit asm functions that take a stride as an argument
; via register, assume that the high dword of that register is filled with 0.
; This is true in practice (since we never do any 64bit arithmetic on strides,
; and x264's strides are all positive), but is not guaranteed by the ABI.

; Name of the .rodata section.
; Kludge: Something on OS X fails to align .rodata even given an align attribute,
; so use a different read-only section.
%macro SECTION_RODATA 0-1 16
    %ifidn __OUTPUT_FORMAT__,macho64
        SECTION .text align=%1
    %elifidn __OUTPUT_FORMAT__,macho
        SECTION .text align=%1
        fakegot:
    %else
        SECTION .rodata align=%1
    %endif
%endmacro

%ifdef WIN64
    %define PIC
%elifndef ARCH_X86_64
; x86_32 doesn't require PIC.
; Some distros prefer shared objects to be PIC, but nothing breaks if
; the code contains a few textrels, so we'll skip that complexity.
    %undef PIC
%endif
%ifdef PIC
    default rel
%endif

; Macros to eliminate most code duplication between x86_32 and x86_64:
; Currently this works only for leaf functions which load all their arguments
; into registers at the start, and make no other use of the stack. Luckily that
; covers most of x264's asm.

; PROLOGUE:
; %1 = number of arguments. loads them from stack if needed.
; %2 = number of registers used. pushes callee-saved regs if needed.
; %3 = number of xmm registers used. pushes callee-saved xmm regs if needed.
; %4 = list of names to define to registers
; PROLOGUE can also be invoked by adding the same options to cglobal

; e.g.
; cglobal foo, 2,3,0, dst, src, tmp
; declares a function (foo), taking two args (dst and src) and one local variable (tmp)

; TODO Some functions can use some args directly from the stack. If they're the
; last args then you can just not declare them, but if they're in the middle
; we need more flexible macro.

; RET:
; Pops anything that was pushed by PROLOGUE

; REP_RET:
; Same, but if it doesn't pop anything it becomes a 2-byte ret, for athlons
; which are slow when a normal ret follows a branch.

; registers:
; rN and rNq are the native-size register holding function argument N
; rNd, rNw, rNb are dword, word, and byte size
; rNm is the original location of arg N (a register or on the stack), dword
; rNmp is native size

%macro DECLARE_REG 6
    %define r%1q %2
    %define r%1d %3
    %define r%1w %4
    %define r%1b %5
    %define r%1m %6
    %ifid %6 ; i.e. it's a register
        %define r%1mp %2
    %elifdef ARCH_X86_64 ; memory
        %define r%1mp qword %6
    %else
        %define r%1mp dword %6
    %endif
    %define r%1  %2
%endmacro

%macro DECLARE_REG_SIZE 2
    %define r%1q r%1
    %define e%1q r%1
    %define r%1d e%1
    %define e%1d e%1
    %define r%1w %1
    %define e%1w %1
    %define r%1b %2
    %define e%1b %2
%ifndef ARCH_X86_64
    %define r%1  e%1
%endif
%endmacro

DECLARE_REG_SIZE ax, al
DECLARE_REG_SIZE bx, bl
DECLARE_REG_SIZE cx, cl
DECLARE_REG_SIZE dx, dl
DECLARE_REG_SIZE si, sil
DECLARE_REG_SIZE di, dil
DECLARE_REG_SIZE bp, bpl

; t# defines for when per-arch register allocation is more complex than just function arguments

%macro DECLARE_REG_TMP 1-*
    %assign %%i 0
    %rep %0
        CAT_XDEFINE t, %%i, r%1
        %assign %%i %%i+1
        %rotate 1
    %endrep
%endmacro

%macro DECLARE_REG_TMP_SIZE 0-*
    %rep %0
        %define t%1q t%1 %+ q
        %define t%1d t%1 %+ d
        %define t%1w t%1 %+ w
        %define t%1b t%1 %+ b
        %rotate 1
    %endrep
%endmacro

DECLARE_REG_TMP_SIZE 0,1,2,3,4,5,6,7,8,9

%ifdef ARCH_X86_64
    %define gprsize 8
%else
    %define gprsize 4
%endif

%macro PUSH 1
    push %1
    %assign stack_offset stack_offset+gprsize
%endmacro

%macro POP 1
    pop %1
    %assign stack_offset stack_offset-gprsize
%endmacro

%macro SUB 2
    sub %1, %2
    %ifidn %1, rsp
        %assign stack_offset stack_offset+(%2)
    %endif
%endmacro

%macro ADD 2
    add %1, %2
    %ifidn %1, rsp
        %assign stack_offset stack_offset-(%2)
    %endif
%endmacro

%macro movifnidn 2
    %ifnidn %1, %2
        mov %1, %2
    %endif
%endmacro

%macro movsxdifnidn 2
    %ifnidn %1, %2
        movsxd %1, %2
    %endif
%endmacro

%macro ASSERT 1
    %if (%1) == 0
        %error assert failed
    %endif
%endmacro

%macro DEFINE_ARGS 0-*
    %ifdef n_arg_names
        %assign %%i 0
        %rep n_arg_names
            CAT_UNDEF arg_name %+ %%i, q
            CAT_UNDEF arg_name %+ %%i, d
            CAT_UNDEF arg_name %+ %%i, w
            CAT_UNDEF arg_name %+ %%i, b
            CAT_UNDEF arg_name %+ %%i, m
            CAT_UNDEF arg_name, %%i
            %assign %%i %%i+1
        %endrep
    %endif

    %assign %%i 0
    %rep %0
        %xdefine %1q r %+ %%i %+ q
        %xdefine %1d r %+ %%i %+ d
        %xdefine %1w r %+ %%i %+ w
        %xdefine %1b r %+ %%i %+ b
        %xdefine %1m r %+ %%i %+ m
        CAT_XDEFINE arg_name, %%i, %1
        %assign %%i %%i+1
        %rotate 1
    %endrep
    %assign n_arg_names %%i
%endmacro

%ifdef WIN64 ; Windows x64 ;=================================================

DECLARE_REG 0, rcx, ecx, cx,  cl,  ecx
DECLARE_REG 1, rdx, edx, dx,  dl,  edx
DECLARE_REG 2, r8,  r8d, r8w, r8b, r8d
DECLARE_REG 3, r9,  r9d, r9w, r9b, r9d
DECLARE_REG 4, rdi, edi, di,  dil, [rsp + stack_offset + 40]
DECLARE_REG 5, rsi, esi, si,  sil, [rsp + stack_offset + 48]
DECLARE_REG 6, rax, eax, ax,  al,  [rsp + stack_offset + 56]
%define r7m [rsp + stack_offset + 64]
%define r8m [rsp + stack_offset + 72]

%macro LOAD_IF_USED 2 ; reg_id, number_of_args
    %if %1 < %2
        mov r%1, [rsp + stack_offset + 8 + %1*8]
    %endif
%endmacro

%macro PROLOGUE 2-4+ 0 ; #args, #regs, #xmm_regs, arg_names...
    ASSERT %2 >= %1
    %assign regs_used %2
    ASSERT regs_used <= 7
    %if regs_used > 4
        push r4
        push r5
        %assign stack_offset stack_offset+16
    %endif
    WIN64_SPILL_XMM %3
    LOAD_IF_USED 4, %1
    LOAD_IF_USED 5, %1
    LOAD_IF_USED 6, %1
    DEFINE_ARGS %4
%endmacro

%macro WIN64_SPILL_XMM 1
    %assign xmm_regs_used %1
    ASSERT xmm_regs_used <= 16
    %if xmm_regs_used > 6
        sub rsp, (xmm_regs_used-6)*16+16
        %assign stack_offset stack_offset+(xmm_regs_used-6)*16+16
        %assign %%i xmm_regs_used
        %rep (xmm_regs_used-6)
            %assign %%i %%i-1
            movdqa [rsp + (%%i-6)*16+8], xmm %+ %%i
        %endrep
    %endif
%endmacro

%macro WIN64_RESTORE_XMM_INTERNAL 1
    %if xmm_regs_used > 6
        %assign %%i xmm_regs_used
        %rep (xmm_regs_used-6)
            %assign %%i %%i-1
            movdqa xmm %+ %%i, [%1 + (%%i-6)*16+8]
        %endrep
        add %1, (xmm_regs_used-6)*16+16
    %endif
%endmacro

%macro WIN64_RESTORE_XMM 1
    WIN64_RESTORE_XMM_INTERNAL %1
    %assign stack_offset stack_offset-(xmm_regs_used-6)*16+16
    %assign xmm_regs_used 0
%endmacro

%macro RET 0
    WIN64_RESTORE_XMM_INTERNAL rsp
    %if regs_used > 4
        pop r5
        pop r4
    %endif
    ret
%endmacro

%macro REP_RET 0
    %if regs_used > 4 || xmm_regs_used > 6
        RET
    %else
        rep ret
    %endif
%endmacro

%elifdef ARCH_X86_64 ; *nix x64 ;=============================================

DECLARE_REG 0, rdi, edi, di,  dil, edi
DECLARE_REG 1, rsi, esi, si,  sil, esi
DECLARE_REG 2, rdx, edx, dx,  dl,  edx
DECLARE_REG 3, rcx, ecx, cx,  cl,  ecx
DECLARE_REG 4, r8,  r8d, r8w, r8b, r8d
DECLARE_REG 5, r9,  r9d, r9w, r9b, r9d
DECLARE_REG 6, rax, eax, ax,  al,  [rsp + stack_offset + 8]
%define r7m [rsp + stack_offset + 16]
%define r8m [rsp + stack_offset + 24]

%macro LOAD_IF_USED 2 ; reg_id, number_of_args
    %if %1 < %2
        mov r%1, [rsp - 40 + %1*8]
    %endif
%endmacro

%macro PROLOGUE 2-4+ ; #args, #regs, #xmm_regs, arg_names...
    ASSERT %2 >= %1
    ASSERT %2 <= 7
    LOAD_IF_USED 6, %1
    DEFINE_ARGS %4
%endmacro

%macro RET 0
    ret
%endmacro

%macro REP_RET 0
    rep ret
%endmacro

%else ; X86_32 ;==============================================================

DECLARE_REG 0, eax, eax, ax, al,   [esp + stack_offset + 4]
DECLARE_REG 1, ecx, ecx, cx, cl,   [esp + stack_offset + 8]
DECLARE_REG 2, edx, edx, dx, dl,   [esp + stack_offset + 12]
DECLARE_REG 3, ebx, ebx, bx, bl,   [esp + stack_offset + 16]
DECLARE_REG 4, esi, esi, si, null, [esp + stack_offset + 20]
DECLARE_REG 5, edi, edi, di, null, [esp + stack_offset + 24]
DECLARE_REG 6, ebp, ebp, bp, null, [esp + stack_offset + 28]
%define r7m [esp + stack_offset + 32]
%define r8m [esp + stack_offset + 36]
%define rsp esp

%macro PUSH_IF_USED 1 ; reg_id
    %if %1 < regs_used
        push r%1
        %assign stack_offset stack_offset+4
    %endif
%endmacro

%macro POP_IF_USED 1 ; reg_id
    %if %1 < regs_used
        pop r%1
    %endif
%endmacro

%macro LOAD_IF_USED 2 ; reg_id, number_of_args
    %if %1 < %2
        mov r%1, [esp + stack_offset + 4 + %1*4]
    %endif
%endmacro

%macro PROLOGUE 2-4+ ; #args, #regs, #xmm_regs, arg_names...
    ASSERT %2 >= %1
    %assign regs_used %2
    ASSERT regs_used <= 7
    PUSH_IF_USED 3
    PUSH_IF_USED 4
    PUSH_IF_USED 5
    PUSH_IF_USED 6
    LOAD_IF_USED 0, %1
    LOAD_IF_USED 1, %1
    LOAD_IF_USED 2, %1
    LOAD_IF_USED 3, %1
    LOAD_IF_USED 4, %1
    LOAD_IF_USED 5, %1
    LOAD_IF_USED 6, %1
    DEFINE_ARGS %4
%endmacro

%macro RET 0
    POP_IF_USED 6
    POP_IF_USED 5
    POP_IF_USED 4
    POP_IF_USED 3
    ret
%endmacro

%macro REP_RET 0
    %if regs_used > 3
        RET
    %else
        rep ret
    %endif
%endmacro

%endif ;======================================================================

%ifndef WIN64
%macro WIN64_SPILL_XMM 1
%endmacro
%macro WIN64_RESTORE_XMM 1
%endmacro
%endif



;=============================================================================
; arch-independent part
;=============================================================================

%assign function_align 16

; Symbol prefix for C linkage
%macro cglobal 1-2+
    %ifndef cglobaled_%1
        %xdefine %1 mangle(program_name %+ _ %+ %1)
        %xdefine %1.skip_prologue %1 %+ .skip_prologue
        CAT_XDEFINE cglobaled_, %1, 1
    %endif
    %ifidn __OUTPUT_FORMAT__,elf
        global %1:function hidden
    %else
        global %1
    %endif
    align function_align
    %1:
    RESET_MM_PERMUTATION ; not really needed, but makes disassembly somewhat nicer
    %assign stack_offset 0
    %if %0 > 1
        PROLOGUE %2
    %endif
%endmacro

%macro cextern 1
    %xdefine %1 mangle(program_name %+ _ %+ %1)
    extern %1
%endmacro

;like cextern, but without the prefix
%macro cextern_naked 1
    %xdefine %1 mangle(%1)
    extern %1
%endmacro

%macro const 2+
    %xdefine %1 mangle(program_name %+ _ %+ %1)
    global %1
    %1: %2
%endmacro

; This is needed for ELF, otherwise the GNU linker assumes the stack is
; executable by default.
%ifidn __OUTPUT_FORMAT__,elf
SECTION .note.GNU-stack noalloc noexec nowrite progbits
%endif
%ifidn __OUTPUT_FORMAT__,elf32
SECTION .note.GNU-stack noalloc noexec nowrite progbits
%endif
%ifidn __OUTPUT_FORMAT__,elf64
SECTION .note.GNU-stack noalloc noexec nowrite progbits
%endif

; merge mmx and sse*

%macro CAT_XDEFINE 3
    %xdefine %1%2 %3
%endmacro

%macro CAT_UNDEF 2
    %undef %1%2
%endmacro

%macro INIT_MMX 0
    %assign avx_enabled 0
    %define RESET_MM_PERMUTATION INIT_MMX
    %define mmsize 8
    %define num_mmregs 8
    %define mova movq
    %define movu movq
    %define movh movd
    %define movnta movntq
    %assign %%i 0
    %rep 8
    CAT_XDEFINE m, %%i, mm %+ %%i
    CAT_XDEFINE nmm, %%i, %%i
    %assign %%i %%i+1
    %endrep
    %rep 8
    CAT_UNDEF m, %%i
    CAT_UNDEF nmm, %%i
    %assign %%i %%i+1
    %endrep
%endmacro

%macro INIT_XMM 0
    %assign avx_enabled 0
    %define RESET_MM_PERMUTATION INIT_XMM
    %define mmsize 16
    %define num_mmregs 8
    %ifdef ARCH_X86_64
    %define num_mmregs 16
    %endif
    %define mova movdqa
    %define movu movdqu
    %define movh movq
    %define movnta movntdq
    %assign %%i 0
    %rep num_mmregs
    CAT_XDEFINE m, %%i, xmm %+ %%i
    CAT_XDEFINE nxmm, %%i, %%i
    %assign %%i %%i+1
    %endrep
%endmacro

%macro INIT_AVX 0
    INIT_XMM
    %assign avx_enabled 1
    %define PALIGNR PALIGNR_SSSE3
    %define RESET_MM_PERMUTATION INIT_AVX
%endmacro

INIT_MMX

; I often want to use macros that permute their arguments. e.g. there's no
; efficient way to implement butterfly or transpose or dct without swapping some
; arguments.
;
; I would like to not have to manually keep track of the permutations:
; If I insert a permutation in the middle of a function, it should automatically
; change everything that follows. For more complex macros I may also have multiple
; implementations, e.g. the SSE2 and SSSE3 versions may have different permutations.
;
; Hence these macros. Insert a PERMUTE or some SWAPs at the end of a macro that
; permutes its arguments. It's equivalent to exchanging the contents of the
; registers, except that this way you exchange the register names instead, so it
; doesn't cost any cycles.

%macro PERMUTE 2-* ; takes a list of pairs to swap
%rep %0/2
    %xdefine tmp%2 m%2
    %xdefine ntmp%2 nm%2
    %rotate 2
%endrep
%rep %0/2
    %xdefine m%1 tmp%2
    %xdefine nm%1 ntmp%2
    %undef tmp%2
    %undef ntmp%2
    %rotate 2
%endrep
%endmacro

%macro SWAP 2-* ; swaps a single chain (sometimes more concise than pairs)
%rep %0-1
%ifdef m%1
    %xdefine tmp m%1
    %xdefine m%1 m%2
    %xdefine m%2 tmp
    CAT_XDEFINE n, m%1, %1
    CAT_XDEFINE n, m%2, %2
%else
    ; If we were called as "SWAP m0,m1" rather than "SWAP 0,1" infer the original numbers here.
    ; Be careful using this mode in nested macros though, as in some cases there may be
    ; other copies of m# that have already been dereferenced and don't get updated correctly.
    %xdefine %%n1 n %+ %1
    %xdefine %%n2 n %+ %2
    %xdefine tmp m %+ %%n1
    CAT_XDEFINE m, %%n1, m %+ %%n2
    CAT_XDEFINE m, %%n2, tmp
    CAT_XDEFINE n, m %+ %%n1, %%n1
    CAT_XDEFINE n, m %+ %%n2, %%n2
%endif
    %undef tmp
    %rotate 1
%endrep
%endmacro

; If SAVE_MM_PERMUTATION is placed at the end of a function and given the
; function name, then any later calls to that function will automatically
; load the permutation, so values can be returned in mmregs.
%macro SAVE_MM_PERMUTATION 1 ; name to save as
    %assign %%i 0
    %rep num_mmregs
    CAT_XDEFINE %1_m, %%i, m %+ %%i
    %assign %%i %%i+1
    %endrep
%endmacro

%macro LOAD_MM_PERMUTATION 1 ; name to load from
    %assign %%i 0
    %rep num_mmregs
    CAT_XDEFINE m, %%i, %1_m %+ %%i
    CAT_XDEFINE n, m %+ %%i, %%i
    %assign %%i %%i+1
    %endrep
%endmacro

%macro call 1
    call %1
    %ifdef %1_m0
        LOAD_MM_PERMUTATION %1
    %endif
%endmacro

; Substitutions that reduce instruction size but are functionally equivalent
%macro add 2
    %ifnum %2
        %if %2==128
            sub %1, -128
        %else
            add %1, %2
        %endif
    %else
        add %1, %2
    %endif
%endmacro

%macro sub 2
    %ifnum %2
        %if %2==128
            add %1, -128
        %else
            sub %1, %2
        %endif
    %else
        sub %1, %2
    %endif
%endmacro

;=============================================================================
; AVX abstraction layer
;=============================================================================

%define sizeofmm0 8
%define sizeofmm1 8
%define sizeofmm2 8
%define sizeofmm3 8
%define sizeofmm4 8
%define sizeofmm5 8
%define sizeofmm6 8
%define sizeofmm7 8
%define sizeofxmm0 16
%define sizeofxmm1 16
%define sizeofxmm2 16
%define sizeofxmm3 16
%define sizeofxmm4 16
%define sizeofxmm5 16
%define sizeofxmm6 16
%define sizeofxmm7 16
%define sizeofxmm8 16
%define sizeofxmm9 16
%define sizeofxmm10 16
%define sizeofxmm11 16
%define sizeofxmm12 16
%define sizeofxmm13 16
%define sizeofxmm14 16
%define sizeofxmm15 16

;%1 == instruction
;%2 == 1 if float, 0 if int
;%3 == 0 if 3-operand (xmm, xmm, xmm), 1 if 4-operand (xmm, xmm, xmm, imm)
;%4 == number of operands given
;%5+: operands
%macro RUN_AVX_INSTR 6-7+
    %if sizeof%5==8
        %define %%regmov movq
    %elif %2
        %define %%regmov movaps
    %else
        %define %%regmov movdqa
    %endif

    %if %4>=3+%3
        %ifnidn %5, %6
            %if avx_enabled && sizeof%5==16
                v%1 %5, %6, %7
            %else
                %%regmov %5, %6
                %1 %5, %7
            %endif
        %else
            %1 %5, %7
        %endif
    %elif %3
        %1 %5, %6, %7
    %else
        %1 %5, %6
    %endif
%endmacro

;%1 == instruction
;%2 == 1 if float, 0 if int
;%3 == 0 if 3-operand (xmm, xmm, xmm), 1 if 4-operand (xmm, xmm, xmm, imm)
%macro AVX_INSTR 3
    %macro %1 2-8 fnord, fnord, fnord, %1, %2, %3
        %ifidn %3, fnord
            RUN_AVX_INSTR %6, %7, %8, 2, %1, %2
        %elifidn %4, fnord
            RUN_AVX_INSTR %6, %7, %8, 3, %1, %2, %3
        %elifidn %5, fnord
            RUN_AVX_INSTR %6, %7, %8, 4, %1, %2, %3, %4
        %else
            RUN_AVX_INSTR %6, %7, %8, 5, %1, %2, %3, %4, %5
        %endif
    %endmacro
%endmacro

AVX_INSTR addpd, 1, 0
AVX_INSTR addps, 1, 0
AVX_INSTR addsd, 1, 0
AVX_INSTR addss, 1, 0
AVX_INSTR addsubpd, 1, 0
AVX_INSTR addsubps, 1, 0
AVX_INSTR andpd, 1, 0
AVX_INSTR andps, 1, 0
AVX_INSTR andnpd, 1, 0
AVX_INSTR andnps, 1, 0
AVX_INSTR blendpd, 1, 0
AVX_INSTR blendps, 1, 0
AVX_INSTR blendvpd, 1, 0
AVX_INSTR blendvps, 1, 0
AVX_INSTR cmppd, 1, 0
AVX_INSTR cmpps, 1, 0
AVX_INSTR cmpsd, 1, 0
AVX_INSTR cmpss, 1, 0
AVX_INSTR divpd, 1, 0
AVX_INSTR divps, 1, 0
AVX_INSTR divsd, 1, 0
AVX_INSTR divss, 1, 0
AVX_INSTR dppd, 1, 0
AVX_INSTR dpps, 1, 0
AVX_INSTR haddpd, 1, 0
AVX_INSTR haddps, 1, 0
AVX_INSTR hsubpd, 1, 0
AVX_INSTR hsubps, 1, 0
AVX_INSTR maxpd, 1, 0
AVX_INSTR maxps, 1, 0
AVX_INSTR maxsd, 1, 0
AVX_INSTR maxss, 1, 0
AVX_INSTR minpd, 1, 0
AVX_INSTR minps, 1, 0
AVX_INSTR minsd, 1, 0
AVX_INSTR minss, 1, 0
AVX_INSTR mpsadbw, 0, 1
AVX_INSTR mulpd, 1, 0
AVX_INSTR mulps, 1, 0
AVX_INSTR mulsd, 1, 0
AVX_INSTR mulss, 1, 0
AVX_INSTR orpd, 1, 0
AVX_INSTR orps, 1, 0
AVX_INSTR packsswb, 0, 0
AVX_INSTR packssdw, 0, 0
AVX_INSTR packuswb, 0, 0
AVX_INSTR packusdw, 0, 0
AVX_INSTR paddb, 0, 0
AVX_INSTR paddw, 0, 0
AVX_INSTR paddd, 0, 0
AVX_INSTR paddq, 0, 0
AVX_INSTR paddsb, 0, 0
AVX_INSTR paddsw, 0, 0
AVX_INSTR paddusb, 0, 0
AVX_INSTR paddusw, 0, 0
AVX_INSTR palignr, 0, 1
AVX_INSTR pand, 0, 0
AVX_INSTR pandn, 0, 0
AVX_INSTR pavgb, 0, 0
AVX_INSTR pavgw, 0, 0
AVX_INSTR pblendvb, 0, 0
AVX_INSTR pblendw, 0, 1
AVX_INSTR pcmpestri, 0, 0
AVX_INSTR pcmpestrm, 0, 0
AVX_INSTR pcmpistri, 0, 0
AVX_INSTR pcmpistrm, 0, 0
AVX_INSTR pcmpeqb, 0, 0
AVX_INSTR pcmpeqw, 0, 0
AVX_INSTR pcmpeqd, 0, 0
AVX_INSTR pcmpeqq, 0, 0
AVX_INSTR pcmpgtb, 0, 0
AVX_INSTR pcmpgtw, 0, 0
AVX_INSTR pcmpgtd, 0, 0
AVX_INSTR pcmpgtq, 0, 0
AVX_INSTR phaddw, 0, 0
AVX_INSTR phaddd, 0, 0
AVX_INSTR phaddsw, 0, 0
AVX_INSTR phsubw, 0, 0
AVX_INSTR phsubd, 0, 0
AVX_INSTR phsubsw, 0, 0
AVX_INSTR pmaddwd, 0, 0
AVX_INSTR pmaddubsw, 0, 0
AVX_INSTR pmaxsb, 0, 0
AVX_INSTR pmaxsw, 0, 0
AVX_INSTR pmaxsd, 0, 0
AVX_INSTR pmaxub, 0, 0
AVX_INSTR pmaxuw, 0, 0
AVX_INSTR pmaxud, 0, 0
AVX_INSTR pminsb, 0, 0
AVX_INSTR pminsw, 0, 0
AVX_INSTR pminsd, 0, 0
AVX_INSTR pminub, 0, 0
AVX_INSTR pminuw, 0, 0
AVX_INSTR pminud, 0, 0
AVX_INSTR pmulhuw, 0, 0
AVX_INSTR pmulhrsw, 0, 0
AVX_INSTR pmulhw, 0, 0
AVX_INSTR pmullw, 0, 0
AVX_INSTR pmulld, 0, 0
AVX_INSTR pmuludq, 0, 0
AVX_INSTR pmuldq, 0, 0
AVX_INSTR por, 0, 0
AVX_INSTR psadbw, 0, 0
AVX_INSTR pshufb, 0, 0
AVX_INSTR psignb, 0, 0
AVX_INSTR psignw, 0, 0
AVX_INSTR psignd, 0, 0
AVX_INSTR psllw, 0, 0
AVX_INSTR pslld, 0, 0
AVX_INSTR psllq, 0, 0
AVX_INSTR pslldq, 0, 0
AVX_INSTR psraw, 0, 0
AVX_INSTR psrad, 0, 0
AVX_INSTR psrlw, 0, 0
AVX_INSTR psrld, 0, 0
AVX_INSTR psrlq, 0, 0
AVX_INSTR psrldq, 0, 0
AVX_INSTR psubb, 0, 0
AVX_INSTR psubw, 0, 0
AVX_INSTR psubd, 0, 0
AVX_INSTR psubq, 0, 0
AVX_INSTR psubsb, 0, 0
AVX_INSTR psubsw, 0, 0
AVX_INSTR psubusb, 0, 0
AVX_INSTR psubusw, 0, 0
AVX_INSTR punpckhbw, 0, 0
AVX_INSTR punpckhwd, 0, 0
AVX_INSTR punpckhdq, 0, 0
AVX_INSTR punpckhqdq, 0, 0
AVX_INSTR punpcklbw, 0, 0
AVX_INSTR punpcklwd, 0, 0
AVX_INSTR punpckldq, 0, 0
AVX_INSTR punpcklqdq, 0, 0
AVX_INSTR pxor, 0, 0
AVX_INSTR shufps, 0, 1
AVX_INSTR subpd, 1, 0
AVX_INSTR subps, 1, 0
AVX_INSTR subsd, 1, 0
AVX_INSTR subss, 1, 0
AVX_INSTR unpckhpd, 1, 0
AVX_INSTR unpckhps, 1, 0
AVX_INSTR unpcklpd, 1, 0
AVX_INSTR unpcklps, 1, 0
AVX_INSTR xorpd, 1, 0
AVX_INSTR xorps, 1, 0
