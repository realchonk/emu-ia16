bits 16
section .bss
global _sys
_sys:
	resw 2

section .text
global _start
extern main, _exit
_start:
	; remember syscall entry point
	mov word [_sys + 0], cx
	mov word [_sys + 2], ax

	mov bx, sp
	lea si, [bx + 2]	; argv
	push si
	push word [bx]		; argc
	call main
	add sp, 4
	push ax
	call _exit
	ud2

