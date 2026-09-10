[cpu 286]
[bits 16]

section .text
global fork
extern _sys
fork:
	mov ax, 9
	call far word [_sys]
	ret
