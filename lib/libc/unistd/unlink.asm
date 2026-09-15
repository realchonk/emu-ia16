[cpu 286]
[bits 16]

section .text
global unlink
extern _sys, _set_errno
unlink:
	mov ax, 6
	call far word [_sys]
	jmp _set_errno
