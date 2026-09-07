[cpu 286]
[bits 16]

section .text
global write
extern _sys
write:
	mov ax, 2
	call far word [_sys]
	ret
