;system call numbers
%define __NR_read 0
%define __NR_write 1
%define __NR_open 2
%define __NR_close 3
%define __NR_brk 12
%define __NR_exit 60
;default file descriptors
%define stdin 0
%define stdout 1
%define stderr 2
;size of buffers
%define _WORDBUFSIZ 32
%define _BUFSIZ 1024
;sugar
%define EXIT_SUCCESS 0
%define EXIT_FAILURE 1

;compatibility with io.inc
%define CMAIN main

%macro _syscall_exit 1
	mov rax, __NR_exit
	mov rdi, %1	; exit code
	syscall
%endmacro

;generic syscall with 3 arguments
%macro _syscall3 4
	push r11
	push rcx
	push rdx
	push rsi
	push rdi
	mov rdx, %4
	mov rsi, %3
	mov rdi, %2
	mov rax, %1
	syscall
	pop rdi
	pop rsi
	pop rdx
	pop rcx
	pop r11
%endmacro


%macro PCALL1 3 ;(function, data, movsx/movzx)
	IS_GPR %2
	push rdi
	push rsi
	push rcx
	push rdx
%if %$is_reg = 1
%if %$reg_size < 8
	%3 rdi, %2
%else
	mov rdi, %2
%endif
%else
%error Register expected
%endif
	call %1
	pop rdx
	pop rcx
	pop rsi
	pop rdi
%endmacro

%macro PCALL0 1 ; (function)
	push rdi
	push rsi
	push rcx
	push rdx
	call %1
	pop rdx
	pop rcx
	pop rsi
	pop rdi
%endmacro

%macro _syscall_write 2 ; (buffer, size)
	_syscall3 __NR_write, stdout, %1, %2
%endmacro

%macro _syscall_read 2 ; (buffer, size)
	_syscall3 __NR_read, stdin, %1, %2
%endmacro

%macro EXIT 0-1 EXIT_SUCCESS
	_syscall_exit %1
%endmacro

; write %2 bytes to memory at %1
%macro WRITE 2
	_syscall_write %1, %2
%endmacro

%macro PRINT_CHAR 1 ;(reg/IM/MEM)
	push rax
	mov al, %1
	mov byte [_char], al
	_syscall_write _char, 1
	pop rax
%endmacro

;If you want to handle errors, call function _getchar instead
%macro GET_CHAR 1
	IS_GPR %1
%if %$is_reg = 0 || %$reg_size != 1
%error Byte register expected
%endif
	push rax
	PCALL0 _getchar
	push rax
	mov rax, [rsp + 8]
	mov %1, [rsp]
	add rsp, 16
%endmacro

%macro NEWLINE 0
	PRINT_CHAR 10
%endmacro

%macro PRINT_STRING 1
	push rax
	push rdi
	mov rdi, %1
	call _strlen
	WRITE %1, rax
	pop rdi
	pop rax
%endmacro

;size, MEM
%macro GET_STRING 2
	push rax
	_syscall_read %2, %1
	mov byte [%2 + rax], 0
	pop rax
%endmacro

%macro READ_STRING 2
	_syscall_read %1, %2
	mov byte [%1 + rax], 0
%endmacro

%macro GET_DEC 1
	IS_GPR %1
%if %$is_reg = 1
	push rax
	PCALL0 _get_dec
	push rax
	mov rax, [rsp + 8]
	mov %1, [rsp]
	add rsp, 16
%else
%error Register expexted
%endif
%endmacro

%macro GET_HEX 1
	IS_GPR %1
%if %$is_reg = 1
	push rax
	PCALL0 _get_hex
	push rax
	mov rax, [rsp + 8]
	mov %1, [rsp]
	add rsp, 16
%else
%error Register expexted
%endif
%endmacro

%macro PRINT_BIN 1
	push rax
	PCALL1 _print_bin, %1, movzx
	pop rax
%endmacro

%macro PRINT_HEX 1
	push rax
	PCALL1 _print_hex, %1, movzx
	pop rax
%endmacro

%macro PRINT_UDEC 1
	push rax
	PCALL1 _print_udec, %1, movzx
	pop rax
%endmacro

%macro PRINT_DEC 1
	push rax
	PCALL1 _print_dec, %1, movsx
	pop rax
%endmacro


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;      functions' implementation      ;
;           starts here.              ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
section .data
_bufpos dq 0
_buflen dq 0
section .bss
_buffer		resb _BUFSIZ ; buffer for storing input, important
_wordbuf	resb _WORDBUFSIZ ; general buffer, assume it's always dirty
_char		resq 1
section .text
; Arguments are passed in registers according to System V AMD64 ABI.
; It's rather tricky, but it's essential to call functions from C library,
; so I guess I'll use this for my functions too.
; You can find more info here:
; https://github.com/hjl-tools/x86-psABI/wiki/X86-psABI
;
; arguments orger: rdi, rsi, rdx, rcx, r8, r9 (callee saved)
; return value is stored in rax and rdx
; rbx, rsp, rbp, r12-r15  caller saved
; st0-st1 used to return long double arguments
; xmm0-xmm7 (xmm0-xmm1) used to pass (return) floating point arguments
; r10-r11, st2-st7 temporary registers
; With variable arguments al is responsible for passing information about
; number of vector registers used. Remember this when calling printf from
; libc.
; For example, printf("%f %d", myfloat, myint) expects format string address
; to be in rdi, myint value to be in rsi, myfloat value to be in xmm0 and
; al must be greater than 0.

; in case you want to play with linking this as a module
%ifdef MODULE
global _print_udec
global _print_dec
global _print_hex
global _print_bin
global _get_dec
global _get_hex
global _getchar
global _putchar
global _strlen
%endif

_putchar: ; (char)
	mov [_char], dil
	_syscall_write _char, 1
	ret ; (error code)
_getchar: ; (void)
	mov rsi, [_bufpos]
	cmp rsi, [_buflen]
	jne .NOT_EMPTY
	READ_STRING _buffer, _BUFSIZ - 1
	test rax, rax
	js .ERR
	mov [_buflen], rax
	xor rsi, rsi
.NOT_EMPTY:
	mov al, [_buffer + rsi]
	inc rsi
	mov [_bufpos], rsi
	ret
.ERR:
	mov qword [_bufpos], 0
	mov qword [_buflen], 0
	mov rax, -1
	ret

_strlen: ; (string pointer)
	xor rax, rax
.L:	cmp byte [rdi], 0
	je .FIN
	inc rax
	inc rdi
	jmp .L
.FIN:
	ret ; (length of string)

;integer to ascii string (binary)
_itoab: ; (value)
	mov rax, _wordbuf + _WORDBUFSIZ
	xor rdx, rdx
.L:	dec rax
	inc rdx
	shr rdi, 1
	jc .ONE
	mov byte [rax], '0'
	jmp .NEXT
.ONE:	mov byte [rax], '1'
.NEXT:	jnz .L
	ret ; (begining of the string; byte count)

;integer to ascii string (hexadecimal)
_itoah: ; (value)
	mov rax, _wordbuf + _WORDBUFSIZ
	xor rdx, rdx
.L:	dec rax
	inc rdx
	mov rcx, rdi
	and cl, 0xF
	or cl, '0'
	cmp cl, '9'
	jbe .NEXT
	add cl, 'A' - '9' - 1
.NEXT:	mov [rax], cl
	shr rdi, 4
	jnz .L
	ret ; (begining of the string; byte count)

;unsigned integer to ascii string
_itoa: ; (value)
	push rbx
	mov rax, rdi
	mov rbx, 10
	mov rdi, _wordbuf + _WORDBUFSIZ
	xor rcx, rcx
.L:	xor rdx, rdx
	div rbx
	or dl, '0'
	dec rdi
	mov [rdi], dl
	inc rcx
	test rax, rax
	jnz .L
	mov rax, rdi
	mov rdx, rcx
	pop rbx
	ret ; (begining of the string; byte count)

_atoi: ; (string pointer)
	xor rdx, rdx
	xor rax, rax
.L:	mov dl, [rdi]
	cmp dl, '0'
	jb .FIN
	cmp dl, '9'
	ja .FIN
	lea rax, [5 * rax]  ; multiplying by 10
	shl rax, 1		; like cool kids
	and dl, 0xF		; char -> digit like a hacker
	add rax, rdx
	inc rdi
	cmp byte [rdi], 0
	jne .L
.FIN	ret ; (value)

_atoih: ; (string pointer)
	xor rdx, rdx
	xor rax, rax
.L:	mov dl, [rdi]
	cmp dl, '0'
	jb .FIN
	cmp dl, '9'
	ja .AtoF
	and dl, 0xF		; char -> digit (0-9)
	jmp .ADD
.AtoF:	and dl, 11011111b	; upper case like a hacker
	sub dl, 'A' - 10	; microoptimise like a boss
	jl .FIN
	test dl, 11110000b
	jnz .FIN
.ADD:	shl rax, 4
	or rax, rdx
	inc rdi
	cmp byte [rdi], 0
	jne .L
.FIN:	ret ; (value)

; read word from stdin to _wordbuf
_get_word: ; (void)
	mov rdi, _wordbuf
.L:	call _getchar
	test rax, rax
	jz .FIN
	js .FIN
	cmp al, 10
	je .FIN
	cmp al, ' '
	je .FIN
	mov [rdi], al
	inc rdi
	cmp rdi, _wordbuf + _WORDBUFSIZ - 1
	jne .L
.FIN:	mov byte [rdi], 0
	ret ; (void)

_get_dec: ; (void)
	call _get_word
	mov rdi, _wordbuf
	cmp byte [_wordbuf], '+'
	jne .NOTPLUS
	inc rdi
	jmp .PLUS
.NOTPLUS:
	cmp byte [_wordbuf], '-'
	je .MINUS
.PLUS:	call _atoi
	jmp .FIN
.MINUS:	inc rdi
	call _atoi
	neg rax
.FIN:	ret ;(value)

_get_hex: ; (void)
	call _get_word
	mov rdi, _wordbuf
	cmp byte [_wordbuf], '+'
	jne .NOTPLUS
	inc rdi
	jmp .PLUS
.NOTPLUS:
	cmp byte [_wordbuf], '-'
	jne .PLUS
	inc rdi
.PLUS:	cmp byte [rdi], '0'
	jne .ATOI
	cmp byte [rdi + 1], 'X'
	je .PREF
	cmp byte [rdi + 1], 'x'
	jne .ATOI
.PREF:	add rdi, 2
.ATOI:	call _atoih
	cmp byte [_wordbuf], '-'
	jne .FIN
	neg rax
.FIN:	ret ; (value)

_print_bin: ; (value)
	call _itoab
	WRITE rax, rdx
	ret ; (error code)

_print_hex: ; (value)
	call _itoah
	WRITE rax, rdx
	ret ; (error code)

_print_udec: ; (value)
	call _itoa
	WRITE rax, rdx
	ret ; (error code)

_print_dec: ; (value)
	test rdi, rdi	; check sign bit
	jns .NOT_NEG
	neg rdi
	call _itoa
	dec rax
	mov byte [rax], '-'
	inc rdx
	jmp .PRINT
.NOT_NEG:
	call _itoa
.PRINT:	WRITE rax, rdx
	ret ; (error code)

;Macros from io.inc
%macro IS_GPR 1.nolist
    %push IS_GPR
    %assign %$is_reg 0
    %assign %$reg_size 1
    %ifidni %1, ah
        %assign %$is_reg 1
    %elifidni %1, al
        %assign %$is_reg 1
    %elifidni %1, bl
        %assign %$is_reg 1
    %elifidni %1, bh
        %assign %$is_reg 1
    %elifidni %1, cl
        %assign %$is_reg 1
    %elifidni %1, ch
        %assign %$is_reg 1
    %elifidni %1, dl
        %assign %$is_reg 1
    %elifidni %1, dh
        %assign %$is_reg 1
    %elifidni %1, spl
        %assign %$is_reg 1
    %elifidni %1, bpl
        %assign %$is_reg 1
    %elifidni %1, dil
        %assign %$is_reg 1
    %elifidni %1, sil
        %assign %$is_reg 1
    %elifidni %1, ax
        %assign %$is_reg 1
        %assign %$reg_size 2
    %elifidni %1, bx
        %assign %$is_reg 1
        %assign %$reg_size 2
    %elifidni %1, cx
        %assign %$is_reg 1
        %assign %$reg_size 2
    %elifidni %1, dx
        %assign %$is_reg 1
        %assign %$reg_size 2
    %elifidni %1, sp
        %assign %$is_reg 1
        %assign %$reg_size 2
    %elifidni %1, bp
        %assign %$is_reg 1
        %assign %$reg_size 2
    %elifidni %1, si
        %assign %$is_reg 1
        %assign %$reg_size 2
    %elifidni %1, di
        %assign %$is_reg 1
        %assign %$reg_size 2
    %elifidni %1, eax
        %assign %$is_reg 1
        %assign %$reg_size 4
    %elifidni %1, ebx
        %assign %$is_reg 1
        %assign %$reg_size 4
    %elifidni %1, ecx
        %assign %$is_reg 1
        %assign %$reg_size 4
    %elifidni %1, edx
        %assign %$is_reg 1
        %assign %$reg_size 4
    %elifidni %1, esp
        %assign %$is_reg 1
        %assign %$reg_size 4
    %elifidni %1, ebp
        %assign %$is_reg 1
        %assign %$reg_size 4
    %elifidni %1, esi
        %assign %$is_reg 1
        %assign %$reg_size 4
    %elifidni %1, edi
        %assign %$is_reg 1
        %assign %$reg_size 4
    %elifidni %1, rax
        %assign %$is_reg 1
        %assign %$reg_size 8
    %elifidni %1, rbx
        %assign %$is_reg 1
        %assign %$reg_size 8
    %elifidni %1, rcx
        %assign %$is_reg 1
        %assign %$reg_size 8
    %elifidni %1, rdx
        %assign %$is_reg 1
        %assign %$reg_size 8
    %elifidni %1, rsp
        %assign %$is_reg 1
        %assign %$reg_size 8
    %elifidni %1, rbp
        %assign %$is_reg 1
        %assign %$reg_size 8
    %elifidni %1, rsi
        %assign %$is_reg 1
        %assign %$reg_size 8
    %elifidni %1, rdi
        %assign %$is_reg 1
        %assign %$reg_size 8
    %elifidni %1, r8
        %assign %$is_reg 1
        %assign %$reg_size 8
    %elifidni %1, r9
        %assign %$is_reg 1
        %assign %$reg_size 8
    %elifidni %1, r10
        %assign %$is_reg 1
        %assign %$reg_size 8
    %elifidni %1, r11
        %assign %$is_reg 1
        %assign %$reg_size 8
    %elifidni %1, r12
        %assign %$is_reg 1
        %assign %$reg_size 8
    %elifidni %1, r13
        %assign %$is_reg 1
        %assign %$reg_size 8
    %elifidni %1, r14
        %assign %$is_reg 1
        %assign %$reg_size 8
    %elifidni %1, r15
        %assign %$is_reg 1
        %assign %$reg_size 8
    %elifidni %1, r8d
        %assign %$is_reg 1
        %assign %$reg_size 4
    %elifidni %1, r9d
        %assign %$is_reg 1
        %assign %$reg_size 4
    %elifidni %1, r10d
        %assign %$is_reg 1
        %assign %$reg_size 4
    %elifidni %1, r11d
        %assign %$is_reg 1
        %assign %$reg_size 4
    %elifidni %1, r12d
        %assign %$is_reg 1
        %assign %$reg_size 4
    %elifidni %1, r13d
        %assign %$is_reg 1
        %assign %$reg_size 4
    %elifidni %1, r14d
        %assign %$is_reg 1
        %assign %$reg_size 4
    %elifidni %1, r15d
        %assign %$is_reg 1
        %assign %$reg_size 4
    %elifidni %1, r8w
        %assign %$is_reg 1
        %assign %$reg_size 2
    %elifidni %1, r9w
        %assign %$is_reg 1
        %assign %$reg_size 2
    %elifidni %1, r10w
        %assign %$is_reg 1
        %assign %$reg_size 2
    %elifidni %1, r11w
        %assign %$is_reg 1
        %assign %$reg_size 2
    %elifidni %1, r12w
        %assign %$is_reg 1
        %assign %$reg_size 2
    %elifidni %1, r13w
        %assign %$is_reg 1
        %assign %$reg_size 2
    %elifidni %1, r14w
        %assign %$is_reg 1
        %assign %$reg_size 2
    %elifidni %1, r15w
        %assign %$is_reg 1
        %assign %$reg_size 2
    %elifidni %1, r8b
        %assign %$is_reg 1
        %assign %$reg_size 1
    %elifidni %1, r9b
        %assign %$is_reg 1
        %assign %$reg_size 1
    %elifidni %1, r10b
        %assign %$is_reg 1
        %assign %$reg_size 1
    %elifidni %1, r11b
        %assign %$is_reg 1
        %assign %$reg_size 1
    %elifidni %1, r12b
        %assign %$is_reg 1
        %assign %$reg_size 1
    %elifidni %1, r13b
        %assign %$is_reg 1
        %assign %$reg_size 1
    %elifidni %1, r14b
        %assign %$is_reg 1
        %assign %$reg_size 1
    %elifidni %1, r15b
        %assign %$is_reg 1
        %assign %$reg_size 1
    %endif
%endmacro

