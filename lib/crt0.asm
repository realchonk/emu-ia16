bits 16
section .bss
global _sys, __progname
_sys:
	resw 2
__progname:
	resw 2

section .text
global _start
extern main, _exit
_start:
	; remember syscall entry point
	mov word [_sys + 0], cx
	mov word [_sys + 2], ax

	mov bx, sp

	; argv
	lea si, [bx + 2]
	push si

	; store argv[0] in __progname
	mov ax, word [si]
	mov word [__progname], ax

	; argc
	push word [bx]

	call main
	add sp, 4
	push ax
	call _exit
	ud2

