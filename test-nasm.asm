%include "yanel.inc"

section .data
HELLO	db "HELLO", 10, 0
section .text
global _start


_start:
	PRINT_STRING HELLO
	GET_DEC rax
	GET_DEC rbx
	add rax, rbx
	PRINT_DEC rax
	NEWLINE
	PRINT_UDEC rax
	NEWLINE
	PRINT_HEX rax
	NEWLINE
	PRINT_BIN rax
	NEWLINE
	EXIT
