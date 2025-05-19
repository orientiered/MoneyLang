;==============================================;
; Stdlib for mpp - money programming language  ;
; @orientiered, MIPT 2025
;==============================================;

section .text

global __stdlib_out
global _start

FLOAT_TOTAL_DIGITS equ 6
BUFFER_LEN equ 18
; ============================================== ;
; Print floating point number to stdout
; Arg:
;   [rsp+8] -- fp number to print
; Destr: xmm0, xmm1, xmm2, r12, r13, r8, rsi, rdx
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
    .int_conv_loop:
        div  rbx

        ; rdx = rax % 10
        ; rax = rax / 10
        lea  rsi, [rdx+'0']
        mov  BYTE [r8 + r12], sil
        xor  rdx, rdx
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
    .float_part_loop:
        div  rbx
        lea  rsi, [rdx+'0']
        mov  BYTE [r8 + rcx + 10], sil
        xor  rdx, rdx

        loop .float_part_loop


    mov rax, 1 ; write syscall
    mov rdi, 1 ; to stdout
    lea rsi, [r8 + r12] ; string start

    mov rdx, BUFFER_LEN
    sub rdx, r12    ; length: BUF_LEN - r12

    syscall

    add rsp, BUFFER_LEN

    pop  rbp
    ret


_start:
    ; mov rax, 0x3FF0000000000000 ; 1
    mov rax, 0x3F7988D2A1F8E3AC ; 1
    push rax
    call __stdlib_out

    mov rax, 0x3c
    mov rdi, 0
    syscall
