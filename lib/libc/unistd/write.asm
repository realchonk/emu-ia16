[cpu 286]
[bits 16]

section .text
global write
extern _sys, _set_errno
write:
	mov ax, 2
	call far word [_sys]
	jmp _set_errno
