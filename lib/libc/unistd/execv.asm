[cpu 286]
[bits 16]

section .text
global execv
extern _sys, _set_errno
execv:
	mov ax, 10
	call far word [_sys]
	jmp _set_errno
