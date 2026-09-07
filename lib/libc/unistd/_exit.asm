[cpu 286]
[bits 16]
section .text
extern _sys
global _exit
_exit:
	mov ax, 1
	call far word [_sys]
