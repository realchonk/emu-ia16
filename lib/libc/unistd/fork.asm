[cpu 286]
[bits 16]

section .text
global fork
extern _sys, _set_errno
fork:
	mov ax, 9
	call far word [_sys]
	jmp _set_errno
