[cpu 286]
[bits 16]

section .text
global unlink
extern _sys
unlink:
	mov ax, 6
	call far word [_sys]
	ret
