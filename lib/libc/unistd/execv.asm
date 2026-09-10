[cpu 286]
[bits 16]

section .text
global execv
extern _sys
execv:
	mov ax, 10
	call far word [_sys]
	ret
