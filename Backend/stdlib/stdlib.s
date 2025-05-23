;==============================================;
; Stdlib for mpp - money programming language  ;
; @orientiered, MIPT 2025
;==============================================;

section .text

global __stdlib_out
global __stdlib_in
global _start

;================================================;
; We store here adresses of stdlib functions
; They are relative to the beginning of .text section
; When reading compiled stdlib binary, we can read them and resolve calls to stdlib
;================================================;
dq __stdlib_out - $
dq __stdlib_in  - $ + 8
;===============================================;

FLOAT_TOTAL_DIGITS equ 6
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


    ;------------- Allocating buffer on stack ---------------------
    BUFFER_LEN equ 18
    ; Buffer layout:
    ;    fractional part           integer part
    ; |\n|- - - - - - - - - - -|.|- - - - - - - - - '1' - - - -|
    ;  ^                        ^                       ^     ^
    ;  |                        |                       |     |
    ; r8+17                   r8+10                 r8+r12    r8
    ;                                    (first symbol after number)
    xor  r12, r12   ; index in buffer
    sub  rsp, BUFFER_LEN ; buffer with chars
    mov  r8,  rsp   ; buffer start

    ;---------- Checking  sign of given number --------------------
    pextrb ecx, xmm0, 7 ; extracting high byte with sign bit
    ;---------- Removing sign from number -------------------------
    mov  rax,  0x7FFFFFFFFFFFFFFF
    movq xmm2, rax ; xmm2 = 0b0111111111...111
    pand xmm0, xmm2

    ;---------- Extracting integer part ---------------------------
    cvttsd2si rax,  xmm0 ; rax = int(x) with rounding towards zero
    cvtsi2sd  xmm1, rax  ; xmm1  = double(int(x))

    ;---------- Printing integer part -----------------------------
    mov  rdi, 10 ; divisor
    mov  r12, 9  ; position in buffer right after '.'
    xor  rdx, rdx ; rdx is used in div instruction, so we must set it 0
    .int_conv_loop:
        div  rdi
        ; rdx = rax % 10
        ; rax = rax / 10

        ;--------- rsi = rdx + '0' = digit char ------
        lea  rsi, [rdx+'0']
        ; zeroing rdx againg
        xor  rdx, rdx

        ;--------- writing digit to the buffer -------
        mov  BYTE [r8 + r12], sil
        dec  r12  ; shiting index

        ;--------- repeat until result is 0 ----------
        test rax, rax
        jnz .int_conv_loop

    ;------------- Writing sign  ---------------------
    test cx,  0x80    ; checking sign bit stored in cx
    jz .skip_minus
        mov BYTE [r8 + r12], '-'
        dec r12
    .skip_minus:

    ;------------- Writing new line symbol and dot -----------
    mov BYTE [r8 + 17], 10; '\n'
    mov BYTE [r8 + 10], '.'

    ;------------- Converting fractional part --------
    subsd xmm0, xmm1;  ; xmm0 = fractional part of x

    mov   rax, 0x412E848000000000  ; 10^6 -> get 6 digits
    movq  xmm2, rax
    mulsd xmm0, xmm2     ; xmm0 = float_part(x) * 10^6

    cvttsd2si rax,  xmm0 ; rax = int(xmm0) with rounding towards zero

    ;------------ Convertion loop --------------------
    mov  rcx, 6     ; processing 6 digits
    ; rdx is alredy zero
    .float_part_loop:
        div  rdi
        lea  rsi, [rdx+'0']
        ;------ Writing digit to the fractional part -----
        mov  BYTE [r8 + rcx + 10], sil
        xor  rdx, rdx

        loop .float_part_loop

    ;------------ Using syscall to write buffer to stdout ------------
    mov rax, 1 ; write syscall
    mov rdi, 1 ; to stdout

    inc r12  ; now r12 is index of first symbol
    lea rsi, [r8 + r12] ; string start

    mov rdx, BUFFER_LEN
    sub rdx, r12    ; length: BUF_LEN - r12

    syscall ; write

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
; Destr: syscall, r13, r8, r15
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

    syscall

    xor rsi, rsi
    mov sil, [r8]
%endmacro

__stdlib_in:
    push rbp
    mov  rbp, rsp

    sub  rsp, 8
    mov  r8, rsp
    lea  r15, [rel __constants_table] ; rip relative addressing for table

    pxor xmm0, xmm0  ; xmm0 = 0
    pxor xmm1, xmm1  ; xmm1 = 0
    movq xmm2, [rel fp1] ; xmm2 = 1

    MACRO_readchar
    xor  r13, r13
    cmp  sil, '-'
    jne  .skipMinus
        mov r13, 1 ; r13 signals if we have minus sign
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
        mulsd xmm0, [rel fp10] ; *= 10
        addsd xmm0, [r15 + rsi * 8]

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
        mulsd xmm1, [rel fp10] ; *= 10
        addsd xmm1, [r15 + rsi * 8]

        ;---------- xmm2 *= 10 -------------------------------;
        mulsd xmm2, [rel fp10] ; xmm3 *= 10

        jmp .floatPart_loop

    .conv_end:

    divsd xmm1, xmm2 ; shifting point to correct position
    addsd xmm0, xmm1 ; result = int part + float part

    test r13, r13
    ;----- Setting sign bit -------------------
    jz .skipNegation
        mov  rax, 0x8000000000000000 ; only sign bit
        movq xmm2, rax
        por  xmm0, xmm2 ; setting sign bit
    .skipNegation:

    movq rax, xmm0

    mov  rsp, rbp
    pop  rbp
    ret
;~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~;
