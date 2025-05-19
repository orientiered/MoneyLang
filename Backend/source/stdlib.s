;==============================================;
; Stdlib for mpp - money programming language  ;
; @orientiered, MIPT 2025
;==============================================;

section .text

global __stdlib_out
global __stdlib_in
global _start

FLOAT_TOTAL_DIGITS equ 6
BUFFER_LEN equ 18
; ============================================== ;
; Print floating point number to stdout
; Arg:
;   [rsp+8] -- fp number to print
; Destr: xmm0, xmm1, xmm2, r12, r13, r8, rsi, rdx, syscall
; ============================================== ;
__stdlib_out:
    push rbp
    mov  rbp, rsp

    movq xmm0, [rbp+16] ; xmm0 = number

    mov  rax,  0x7FFFFFFFFFFFFFFF
    movq xmm2, rax ; xmm2 = 0b0111111111...111

    xor  r12, r12   ; index in buffer
    sub  rsp, BUFFER_LEN ; buffer with chars
    mov  r8,  rsp   ; buffer start

; Checking sign of given number
    pextrb ecx, xmm0, 7 ; extracting high byte with sign bit

    pand xmm0, xmm2

    cvttsd2si rax,  xmm0 ; rax = int(x) with rounding towards zero
    cvtsi2sd  xmm1, rax  ; xmm1  = double(int(x))

    mov  rbx, 10
    mov  r12, 9
    xor  rdx, rdx
    .int_conv_loop:
        div  rbx

        ; rdx = rax % 10
        ; rax = rax / 10
        lea  rsi, [rdx+'0']
        xor  rdx, rdx

        mov  BYTE [r8 + r12], sil
        dec  r12

        test rax, rax
        jnz .int_conv_loop

    test cx,  0x80    ; checking sign bit
    jz .skip_minus
        mov BYTE [r8 + r12], '-'
        dec r12
    .skip_minus:

    mov BYTE [r8+17], 10; '\n'

    subsd xmm0, xmm1;
    mov   rax, 0x412E848000000000  ; 10^6
    movq  xmm2, rax
    mulsd xmm0, xmm2     ; xmm0 = float_part(x) * 10^6
    cvttsd2si rax,  xmm0 ; rax = int(x) with rounding towards zero
    cvtsi2sd  xmm1, rax  ;

    mov  BYTE [r8 + 10], '.'
    mov  rcx, 6
    xor  rdx, rdx
    .float_part_loop:
        div  rbx
        lea  rsi, [rdx+'0']
        mov  BYTE [r8 + rcx + 10], sil
        xor  rdx, rdx

        loop .float_part_loop


    mov rax, 1 ; write syscall
    mov rdi, 1 ; to stdout
    lea rsi, [r8 + r12+1] ; string start

    mov rdx, BUFFER_LEN
    sub rdx, r12    ; length: BUF_LEN - r12

    syscall

    mov  rsp, rbp
    pop  rbp
    ret
;~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~;

;======================================================;
; Read floating point number from stdin
; Args:
;   none
; Ret:
;   rax - scanned floating point number
; Destr: syscall
;======================================================;

__constants_table:
fp0 dq 0.0
fp1 dq 1.0
fp2 dq 2.0
fp3 dq 3.0
fp4 dq 4.0
fp5 dq 5.0
fp6 dq 6.0
fp7 dq 7.0
fp8 dq 8.0
fp9 dq 9.0
fp10 dq 10.0

%macro MACRO_readchar 0
    mov rax, 0
    mov rdi, 0   ; reading from stdin
    mov rsi, r8 ; read to buffer in rsp
    mov rdx, 1   ; one symbol

    ; sub  rsp, 24
    ; movq [rsp],    xmm0
    ; movq [rsp+8],  xmm1
    ; movq [rsp+16], xmm2
    syscall
    ; movq xmm0, [rsp]
    ; movq xmm1, [rsp+8]
    ; movq xmm2, [rsp+16]
    ; add  rsp, 24

    xor rsi, rsi
    mov sil, [r8]
%endmacro

__stdlib_in:
    push rbp
    mov  rbp, rsp

    sub  rsp, 8
    mov  r8, rsp

    pxor xmm0, xmm0  ; xmm0 = 0
    pxor xmm1, xmm1  ; xmm1 = 0
    movq xmm2, [fp1] ; xmm2 = 1

    MACRO_readchar
    cmp  sil, '-'
    jne  .skipMinus
        mov BYTE [rsp+1], 1 ; [rsp+1] signals if we have minus sign
        MACRO_readchar
    .skipMinus:

    ;------------Processing integer part-----------------------------;
    .intPart_loop:
        ;------------ stop on space or new line --------------;
        cmp  sil, ' '
        je   .conv_end
        cmp  sil, 10 ; \n
        je   .conv_end

        ;---------- process float part -----------------------;
        cmp  sil, '.'
        je  .floatPart_loop

        ;---------- wrong symbol passed ----------------------;
        ; stop if symbol < '0'
        sub  sil, '0'
        jb   .conv_end
        cmp  sil,  10
        jae  .conv_end

        ;---------- xmm0 = xmm0 * 10 + digit -----------------;
        mulsd xmm0, [fp10] ; *= 10
        addsd xmm0, [__constants_table + rsi * 8]

        ;---------- reading new char -------------------------;
        MACRO_readchar
        jmp .intPart_loop

    ;------------Processing float part-----------------------------;
    .floatPart_loop:
        ;------------- reading new char --------------------;
        MACRO_readchar

        ;------------ stop on space or new line --------------;
        cmp  sil, ' '
        je   .conv_end
        cmp  sil, 10 ; \n
        je   .conv_end

        ;---------- wrong symbol passed ----------------------;
        ; stop if symbol < '0'
        sub  sil, '0'
        jb   .conv_end
        cmp  sil,  10
        jae  .conv_end

        ;---------- xmm1 = xmm1 * 10 + digit------------------;
        mulsd xmm1, [fp10] ; *= 10
        addsd xmm1, [__constants_table + rsi * 8]

        ;---------- xmm2 *= 10 -------------------------------;
        mulsd xmm2, [fp10] ; xmm3 *= 10

        jmp .floatPart_loop

    .conv_end:

    divsd xmm1, xmm2 ; shifting point to correct position
    addsd xmm0, xmm1 ; result = int part + float part

    movq rax, xmm0

    mov  rsp, rbp
    pop  rbp
    ret
;~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~;

; _start:
;     ; mov rax, 0x3FF0000000000000 ; 1
;     ; mov rax, 0x3F7988D2A1F8E3AC ; 1
;     mov rax, 0x4046E3D70A3D70A4; 45.78
;     push rax
;     call __stdlib_out
;     pop  rax
;
;     call __stdlib_in
;
;     ; movq  xmm0, rax
;     ; mulsd xmm0, [fp2]
;     ; movq  rax, xmm0
;     push  rax
;
;     call __stdlib_out
;
;     pop  rax
;     mov rax, 0x4046E3D70A3D70A4; 45.78
;     push rax
;     call __stdlib_out
;     pop  rax
;
;     mov rax, 0x3c
;     mov rdi, 0
;     syscall
